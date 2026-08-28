#pragma once

#include <gnuradio/block.h>
#include <pmt/pmt.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

class FrameMonitor final : public gr::block
{
public:
    using sptr = std::shared_ptr<FrameMonitor>;
    using Callback = std::function<void(const std::string&)>;
    using PayloadCallback = std::function<void(const std::vector<std::uint8_t>&)>;

    enum class CandidatePriority {
        OpenHoshimi = 0,
        Original = 1,
    };

    static sptr make(Callback callback, PayloadCallback payloadCallback = {},
                     PayloadCallback localCandidateCallback = {});
    std::uint64_t frameCount() const noexcept;
    std::uint64_t primaryFrameCount() const noexcept;
    std::uint64_t openHoshimiFrameCount() const noexcept;
    std::uint64_t suppressedDuplicateCount() const noexcept;
    double secondsSinceFrame() const noexcept;
    ~FrameMonitor() override;
    int general_work(int noutput_items,
                     gr_vector_int& ninput_items,
                     gr_vector_const_void_star& input_items,
                     gr_vector_void_star& output_items) override;

private:
    FrameMonitor(Callback callback, PayloadCallback payloadCallback,
                 PayloadCallback localCandidateCallback);
    void handle(const pmt::pmt_t& message, const char* path);
    void handleLocalCandidate(const pmt::pmt_t& message,
                              CandidatePriority priority);
    void candidateWorker();
    static std::uint32_t candidateKey(const std::vector<std::uint8_t>& payload);
    static std::string describePdu(const pmt::pmt_t& message);

    Callback callback_;
    PayloadCallback payload_callback_;
    PayloadCallback local_candidate_callback_;
    struct RecentFrame {
        std::int64_t sent_ms;
        std::vector<std::uint8_t> payload;
    };

    // GNU Radio message handlers may be entered concurrently from the main,
    // diversity and Viterbi branches.  This lock makes the first complete
    // copy win atomically before any network or SatNOGS output is invoked.
    std::mutex dispatch_mutex_;
    std::deque<RecentFrame> recently_sent_;
    std::atomic<std::uint64_t> frame_count_{0};
    std::atomic<std::uint64_t> primary_frame_count_{0};
    std::atomic<std::uint64_t> openhoshimi_frame_count_{0};
    std::atomic<std::uint64_t> suppressed_duplicate_count_{0};
    std::atomic<std::int64_t> last_frame_ms_{0};

    struct PendingCandidate {
        CandidatePriority priority = CandidatePriority::Original;
        std::vector<std::uint8_t> payload;
        std::chrono::steady_clock::time_point deadline;
    };
    std::mutex candidate_mutex_;
    std::condition_variable candidate_cv_;
    std::map<std::uint32_t, PendingCandidate> pending_candidates_;
    bool stop_candidate_worker_ = false;
    std::thread candidate_worker_;
};
