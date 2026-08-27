#include "frame_monitor.h"

#include <gnuradio/io_signature.h>
#include <iomanip>
#include <limits>
#include <sstream>

namespace {
std::int64_t nowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}
}

FrameMonitor::sptr FrameMonitor::make(Callback callback, PayloadCallback payloadCallback)
{
    return std::shared_ptr<FrameMonitor>(
        new FrameMonitor(std::move(callback), std::move(payloadCallback)));
}

FrameMonitor::FrameMonitor(Callback callback, PayloadCallback payloadCallback)
    : gr::block("frame_monitor", gr::io_signature::make(0, 0, 0),
                gr::io_signature::make(0, 0, 0)),
      callback_(std::move(callback)),
      payload_callback_(std::move(payloadCallback))
{
    for (const char* name : { "primary", "parallel" }) {
        const auto port = pmt::intern(name);
        message_port_register_in(port);
        set_msg_handler(port, [this, name](pmt::pmt_t msg) { handle(msg, name); });
    }
    message_port_register_out(pmt::intern("out"));
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
    std::lock_guard<std::mutex> dispatchLock(dispatch_mutex_);

    last_frame_ms_.store(timestamp, std::memory_order_relaxed);
    const auto count = frame_count_.fetch_add(1, std::memory_order_relaxed) + 1;
    if (std::string(path) == "parallel")
        parallel_frame_count_.fetch_add(1, std::memory_order_relaxed);
    else
        primary_frame_count_.fetch_add(1, std::memory_order_relaxed);
    // Publish first. Hex formatting and UI/SatNOGS callbacks must never sit in
    // front of the low-latency proxy path.
    message_port_pub(pmt::intern("out"), message);
    if (callback_)
        callback_("FEC frame #" + std::to_string(count) + " [" + path + "]\n" +
                  describePdu(message));
    if (payload_callback_)
        payload_callback_(payload);
}

std::uint64_t FrameMonitor::frameCount() const noexcept
{
    return frame_count_.load(std::memory_order_relaxed);
}

std::uint64_t FrameMonitor::primaryFrameCount() const noexcept
{
    return primary_frame_count_.load(std::memory_order_relaxed);
}

std::uint64_t FrameMonitor::parallelFrameCount() const noexcept
{
    return parallel_frame_count_.load(std::memory_order_relaxed);
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
