#pragma once

#ifdef _WIN32

#include <gnuradio/sync_block.h>

#include <windows.h>
#include <mmsystem.h>

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

class WinmmIqSource final : public gr::sync_block
{
public:
    using sptr = std::shared_ptr<WinmmIqSource>;

    static sptr make(int sampleRate, int deviceId, int channels,
                     bool swapIq = false);
    WinmmIqSource(int sampleRate, int deviceId, int channels, bool swapIq);
    ~WinmmIqSource() override;

    bool stop() override;
    int work(int noutputItems, gr_vector_const_void_star& inputItems,
             gr_vector_void_star& outputItems) override;

    std::uint64_t droppedFrames() const noexcept;
    int captureError() const noexcept;

private:
    static void CALLBACK callback(HWAVEIN input, UINT message,
                                  DWORD_PTR instance, DWORD_PTR header,
                                  DWORD_PTR reserved);
    void acceptBuffer(WAVEHDR* header);
    MMRESULT closeDevice() noexcept;

    static constexpr int kBufferCount = 8;
    static constexpr std::size_t kMaximumQueuedFrames = 48000;

    int sample_rate_ = 0;
    int channels_ = 0;
    bool swap_iq_ = false;
    HWAVEIN input_ = nullptr;
    WAVEFORMATEX format_{};
    std::array<WAVEHDR, kBufferCount> headers_{};
    std::array<std::vector<std::int16_t>, kBufferCount> buffers_;
    std::deque<std::int16_t> samples_;
    std::mutex queue_mutex_;
    std::mutex device_mutex_;
    std::condition_variable queue_ready_;
    std::atomic<bool> stopping_{false};
    std::atomic<int> capture_error_{MMSYSERR_NOERROR};
    std::atomic<std::uint64_t> dropped_frames_{0};
};

#endif
