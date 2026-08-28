#include "frame_monitor.h"

#include <gnuradio/io_signature.h>
#include <algorithm>
#include <iomanip>
#include <limits>
#include <sstream>

namespace {
constexpr std::int64_t kDuplicateWindowMs = 2000;
constexpr auto kCandidateArbitrationWindow = std::chrono::milliseconds(50);

std::int64_t nowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}
}

FrameMonitor::sptr FrameMonitor::make(Callback callback,
                                      PayloadCallback payloadCallback,
                                      PayloadCallback localCandidateCallback)
{
    return std::shared_ptr<FrameMonitor>(
        new FrameMonitor(std::move(callback), std::move(payloadCallback),
                         std::move(localCandidateCallback)));
}

FrameMonitor::FrameMonitor(Callback callback, PayloadCallback payloadCallback,
                           PayloadCallback localCandidateCallback)
    : gr::block("frame_monitor", gr::io_signature::make(0, 0, 0),
                gr::io_signature::make(0, 0, 0)),
      callback_(std::move(callback)),
      payload_callback_(std::move(payloadCallback)),
      local_candidate_callback_(std::move(localCandidateCallback))
{
    for (const char* name : { "primary", "openhoshimi" }) {
        const auto port = pmt::intern(name);
        message_port_register_in(port);
        set_msg_handler(port, [this, name](pmt::pmt_t msg) { handle(msg, name); });
    }
    for (const auto& entry : {
             std::pair{"local_openhoshimi", CandidatePriority::OpenHoshimi},
             std::pair{"local_original", CandidatePriority::Original},
         }) {
        const auto port = pmt::intern(entry.first);
        message_port_register_in(port);
        set_msg_handler(port, [this, priority = entry.second](pmt::pmt_t msg) {
            handleLocalCandidate(msg, priority);
        });
    }
    message_port_register_out(pmt::intern("out"));
    candidate_worker_ = std::thread([this] { candidateWorker(); });
}

FrameMonitor::~FrameMonitor()
{
    {
        std::lock_guard<std::mutex> lock(candidate_mutex_);
        stop_candidate_worker_ = true;
        pending_candidates_.clear();
    }
    candidate_cv_.notify_all();
    if (candidate_worker_.joinable())
        candidate_worker_.join();
}

void FrameMonitor::handleLocalCandidate(const pmt::pmt_t& message,
                                        CandidatePriority priority)
{
    if (!local_candidate_callback_)
        return;
    pmt::pmt_t data = pmt::is_pair(message) ? pmt::cdr(message) : message;
    if (!pmt::is_u8vector(data))
        return;
    std::size_t length = 0;
    const auto* bytes = pmt::u8vector_elements(data, length);
    std::vector<std::uint8_t> payload(bytes, bytes + length);
    std::lock_guard<std::mutex> lock(candidate_mutex_);
    const auto now = std::chrono::steady_clock::now();
    const auto key = candidateKey(payload);
    auto [it, inserted] = pending_candidates_.try_emplace(key);
    if (inserted) {
        it->second.deadline = now + kCandidateArbitrationWindow;
        it->second.priority = priority;
        it->second.payload = std::move(payload);
    } else if (priority < it->second.priority) {
        it->second.priority = priority;
        it->second.payload = std::move(payload);
    }
    candidate_cv_.notify_all();
}

void FrameMonitor::candidateWorker()
{
    std::unique_lock<std::mutex> lock(candidate_mutex_);
    while (!stop_candidate_worker_) {
        candidate_cv_.wait(lock, [this] {
            return stop_candidate_worker_ || !pending_candidates_.empty();
        });
        if (stop_candidate_worker_)
            break;
        const auto earliest = std::min_element(
            pending_candidates_.begin(), pending_candidates_.end(),
            [](const auto& left, const auto& right) {
                return left.second.deadline < right.second.deadline;
            })->second.deadline;
        candidate_cv_.wait_until(lock, earliest);
        if (stop_candidate_worker_)
            break;
        const auto now = std::chrono::steady_clock::now();
        std::vector<std::vector<std::uint8_t>> ready;
        for (auto it = pending_candidates_.begin();
             it != pending_candidates_.end();) {
            if (it->second.deadline <= now) {
                ready.push_back(std::move(it->second.payload));
                it = pending_candidates_.erase(it);
            } else {
                ++it;
            }
        }
        if (ready.empty())
            continue;
        lock.unlock();
        if (local_candidate_callback_) {
            for (const auto& payload : ready)
                local_candidate_callback_(payload);
        }
        lock.lock();
    }
}

std::uint32_t FrameMonitor::candidateKey(
    const std::vector<std::uint8_t>& payload)
{
    if (payload.size() >= 8) {
        // CCSDS short header is 5 bytes; a DSLWP SSDV packet then starts with
        // image_id and a 16-bit packet_id. This identity remains stable when
        // replay runs faster than the 50 ms arbitration window.
        return (std::uint32_t(payload[5]) << 16) |
               (std::uint32_t(payload[6]) << 8) | payload[7];
    }
    std::uint32_t key = 2166136261u;
    for (const auto byte : payload)
        key = (key ^ byte) * 16777619u;
    return key;
}

void FrameMonitor::handle(const pmt::pmt_t& message, const char* path)
{
    pmt::pmt_t data = pmt::is_pair(message) ? pmt::cdr(message) : message;
    if (!pmt::is_u8vector(data))
        return;

    std::size_t length = 0;
    const auto* bytes = pmt::u8vector_elements(data, length);
    std::vector<std::uint8_t> payload(bytes, bytes + length);
    const auto timestamp = nowMs();
    // A successful FEC result always wins over every failed local candidate.
    // Upload remains immediate; only failed SSDV fallback waits 50 ms for the
    // other decoder branches to finish.
    {
        std::lock_guard<std::mutex> candidateLock(candidate_mutex_);
        pending_candidates_.erase(candidateKey(payload));
    }
    candidate_cv_.notify_all();
    std::lock_guard<std::mutex> dispatchLock(dispatch_mutex_);

    last_frame_ms_.store(timestamp, std::memory_order_relaxed);
    const auto count = frame_count_.fetch_add(1, std::memory_order_relaxed) + 1;
    if (std::string(path) == "openhoshimi")
        openhoshimi_frame_count_.fetch_add(1, std::memory_order_relaxed);
    else
        primary_frame_count_.fetch_add(1, std::memory_order_relaxed);

    while (!recently_sent_.empty() &&
           timestamp - recently_sent_.front().sent_ms > kDuplicateWindowMs) {
        recently_sent_.pop_front();
    }
    const bool duplicate = std::any_of(
        recently_sent_.begin(), recently_sent_.end(),
        [&payload](const RecentFrame& frame) { return frame.payload == payload; });
    if (!duplicate) {
        recently_sent_.push_back({timestamp, payload});
        // Publish before formatting/logging: the first decoder branch to
        // complete a frame remains the minimum-latency proxy path.
        // The uploader reads the frame from a fixed 10-byte offset in the
        // serialized PMT, so every transport uses a metadata-free envelope.
        message_port_pub(pmt::intern("out"), pmt::cons(pmt::PMT_NIL, data));
        if (payload_callback_)
            payload_callback_(payload);
    } else {
        suppressed_duplicate_count_.fetch_add(1, std::memory_order_relaxed);
    }
    if (callback_)
        callback_("FEC frame #" + std::to_string(count) + " [" + path +
                  (duplicate ? ", duplicate suppressed]\n" : "]\n") +
                  describePdu(message));
}

std::uint64_t FrameMonitor::frameCount() const noexcept
{
    return frame_count_.load(std::memory_order_relaxed);
}

std::uint64_t FrameMonitor::primaryFrameCount() const noexcept
{
    return primary_frame_count_.load(std::memory_order_relaxed);
}

std::uint64_t FrameMonitor::openHoshimiFrameCount() const noexcept
{
    return openhoshimi_frame_count_.load(std::memory_order_relaxed);
}

std::uint64_t FrameMonitor::suppressedDuplicateCount() const noexcept
{
    return suppressed_duplicate_count_.load(std::memory_order_relaxed);
}

double FrameMonitor::secondsSinceFrame() const noexcept
{
    const auto last = last_frame_ms_.load(std::memory_order_relaxed);
    if (last == 0)
        return std::numeric_limits<double>::infinity();
    return (nowMs() - last) / 1000.0;
}

int FrameMonitor::general_work(int,
                               gr_vector_int&,
                               gr_vector_const_void_star&,
                               gr_vector_void_star&)
{
    return 0;
}

std::string FrameMonitor::describePdu(const pmt::pmt_t& message)
{
    pmt::pmt_t data = pmt::is_pair(message) ? pmt::cdr(message) : message;
    if (!pmt::is_u8vector(data))
        return pmt::write_string(message);

    std::size_t length = 0;
    const auto* bytes = pmt::u8vector_elements(data, length);
    const pmt::pmt_t metadata = pmt::is_pair(message)
                                    ? pmt::car(message)
                                    : pmt::PMT_NIL;
    std::ostringstream out;
    out << "***** VERBOSE PDU DEBUG PRINT *****\n"
        << pmt::write_string(metadata) << '\n'
        << "pdu length = " << std::setw(10) << std::setfill(' ') << length
        << " bytes\n"
        << "pdu vector contents =\n";
    for (std::size_t offset = 0; offset < length; offset += 16) {
        out << std::hex << std::setw(4) << std::setfill('0') << offset << ':';
        const std::size_t end = std::min<std::size_t>(offset + 16, length);
        for (std::size_t i = offset; i < end; ++i)
            out << ' ' << std::setw(2) << unsigned(bytes[i]);
        if (end < length)
            out << '\n';
    }
    return out.str();
}
