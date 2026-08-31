#pragma once

#include <gnuradio/sync_block.h>

#include <QString>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

class AsyncWavRecorderSink final : public gr::sync_block
{
public:
    using sptr = std::shared_ptr<AsyncWavRecorderSink>;
    static constexpr std::uint32_t kDefaultMaximumDataBytes = 0xFFF00000U;

    static sptr make(const QString& path, int channels, int sampleRate,
                     std::uint32_t maximumDataBytes = kDefaultMaximumDataBytes);
    AsyncWavRecorderSink(const QString& path, int channels, int sampleRate,
                         std::uint32_t maximumDataBytes = kDefaultMaximumDataBytes);
    ~AsyncWavRecorderSink() override;

    bool stop() override;
    int work(int noutputItems, gr_vector_const_void_star& inputItems,
             gr_vector_void_star& outputItems) override;

    std::uint64_t droppedFrames() const noexcept;
    bool writeFailed() const noexcept;

private:
    void writerLoop();
    void finalize() noexcept;
    bool openSegment() noexcept;
    bool finalizeSegment() noexcept;
    bool writeHeader(std::uint32_t dataBytes) noexcept;
    QString segmentPath() const;

    static constexpr std::size_t kMaximumQueuedFrames = 96000;

    FILE* file_ = nullptr;
    QString original_path_;
    int channels_ = 0;
    int sample_rate_ = 0;
    std::uint32_t maximum_data_bytes_ = 0;
    std::uint32_t segment_data_bytes_ = 0;
    int segment_index_ = 1;
    std::mutex queue_mutex_;
    std::condition_variable queue_ready_;
    std::deque<std::vector<std::int16_t>> chunks_;
    std::size_t queued_frames_ = 0;
    std::thread writer_;
    std::atomic<bool> stopping_{false};
    std::atomic<bool> finalized_{false};
    std::atomic<bool> write_failed_{false};
    std::atomic<std::uint64_t> dropped_frames_{0};
};
