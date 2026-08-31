#include "winmm_iq_source.h"

#ifdef _WIN32

#include <gnuradio/io_signature.h>
#include <gnuradio/sptr_magic.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <stdexcept>

WinmmIqSource::sptr WinmmIqSource::make(int sampleRate, int deviceId,
                                       int channels, bool swapIq)
{
    return gnuradio::make_block_sptr<WinmmIqSource>(
        sampleRate, deviceId, channels, swapIq);
}

WinmmIqSource::WinmmIqSource(int sampleRate, int deviceId,
                             int channels, bool swapIq)
    : gr::sync_block("asrtu_winmm_iq_source",
                     gr::io_signature::make(0, 0, 0),
                     gr::io_signature::make(channels, channels, sizeof(float))),
      sample_rate_(sampleRate), channels_(channels), swap_iq_(swapIq)
{
    if (sampleRate <= 0 || (channels != 1 && channels != 2))
        throw std::invalid_argument("invalid WinMM input format");

    format_.wFormatTag = WAVE_FORMAT_PCM;
    format_.nChannels = WORD(channels);
    format_.nSamplesPerSec = DWORD(sampleRate);
    format_.wBitsPerSample = 16;
    format_.nBlockAlign = WORD(format_.nChannels * format_.wBitsPerSample / 8);
    format_.nAvgBytesPerSec = format_.nSamplesPerSec * format_.nBlockAlign;

    const UINT selected = deviceId < 0 ? WAVE_MAPPER : UINT(deviceId);
    MMRESULT result = waveInOpen(
        &input_, selected, &format_, DWORD_PTR(&WinmmIqSource::callback),
        DWORD_PTR(this), CALLBACK_FUNCTION);
    if (result != MMSYSERR_NOERROR) {
        input_ = nullptr;
        throw std::runtime_error("waveInOpen failed, error code: " +
                                 std::to_string(result));
    }

    const int framesPerBuffer = std::max(sampleRate / 50, 256);
    for (int index = 0; index < kBufferCount; ++index) {
        auto& samples = buffers_[std::size_t(index)];
        auto& header = headers_[std::size_t(index)];
        samples.resize(std::size_t(framesPerBuffer) * std::size_t(channels_));
        std::memset(&header, 0, sizeof(header));
        header.lpData = reinterpret_cast<LPSTR>(samples.data());
        header.dwBufferLength = DWORD(samples.size() * sizeof(std::int16_t));
        result = waveInPrepareHeader(input_, &header, sizeof(header));
        if (result == MMSYSERR_NOERROR)
            result = waveInAddBuffer(input_, &header, sizeof(header));
        if (result != MMSYSERR_NOERROR) {
            capture_error_.store(int(result), std::memory_order_release);
            closeDevice();
            throw std::runtime_error("Unable to prepare WinMM input, error code: " +
                                     std::to_string(result));
        }
    }
    result = waveInStart(input_);
    if (result != MMSYSERR_NOERROR) {
        capture_error_.store(int(result), std::memory_order_release);
        closeDevice();
        throw std::runtime_error("waveInStart failed, error code: " +
                                 std::to_string(result));
    }
}

WinmmIqSource::~WinmmIqSource()
{
    closeDevice();
}

bool WinmmIqSource::stop()
{
    const MMRESULT result = closeDevice();
    if (result != MMSYSERR_NOERROR) {
        capture_error_.store(int(result), std::memory_order_release);
        throw std::runtime_error("Unable to close WinMM input safely, error code: " +
                                 std::to_string(result));
    }
    return true;
}

void CALLBACK WinmmIqSource::callback(HWAVEIN, UINT message,
                                      DWORD_PTR instance, DWORD_PTR header,
                                      DWORD_PTR)
{
    if (message != WIM_DATA || instance == 0 || header == 0)
        return;
    auto* self = reinterpret_cast<WinmmIqSource*>(instance);
    if (!self->stopping_.load(std::memory_order_acquire))
        self->acceptBuffer(reinterpret_cast<WAVEHDR*>(header));
}

void WinmmIqSource::acceptBuffer(WAVEHDR* header)
{
    const std::size_t sampleCount =
        std::size_t(header->dwBytesRecorded) / sizeof(std::int16_t);
    const auto* inputSamples = reinterpret_cast<const std::int16_t*>(header->lpData);
    if (sampleCount >= std::size_t(channels_)) {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        const std::size_t incomingFrames = sampleCount / std::size_t(channels_);
        const std::size_t queuedFrames = samples_.size() / std::size_t(channels_);
        if (queuedFrames + incomingFrames > kMaximumQueuedFrames) {
            const std::size_t removeFrames = std::min(
                queuedFrames, queuedFrames + incomingFrames - kMaximumQueuedFrames);
            for (std::size_t index = 0;
                 index < removeFrames * std::size_t(channels_); ++index)
                samples_.pop_front();
            dropped_frames_.fetch_add(removeFrames, std::memory_order_relaxed);
        }
        samples_.insert(samples_.end(), inputSamples,
                        inputSamples + sampleCount);
    }
    queue_ready_.notify_one();

    if (stopping_.load(std::memory_order_acquire))
        return;
    header->dwBytesRecorded = 0;
    const MMRESULT result = waveInAddBuffer(input_, header, sizeof(*header));
    if (result != MMSYSERR_NOERROR) {
        capture_error_.store(int(result), std::memory_order_release);
        queue_ready_.notify_all();
    }
}

MMRESULT WinmmIqSource::closeDevice() noexcept
{
    std::lock_guard<std::mutex> deviceLock(device_mutex_);
    if (!input_)
        return MMSYSERR_NOERROR;
    stopping_.store(true, std::memory_order_release);
    queue_ready_.notify_all();
    MMRESULT firstError = waveInStop(input_);
    const MMRESULT resetResult = waveInReset(input_);
    if (firstError == MMSYSERR_NOERROR)
        firstError = resetResult;
    for (auto& header : headers_) {
        if ((header.dwFlags & WHDR_PREPARED) != 0) {
            const MMRESULT result =
                waveInUnprepareHeader(input_, &header, sizeof(header));
            if (firstError == MMSYSERR_NOERROR)
                firstError = result;
        }
    }
    const MMRESULT closeResult = waveInClose(input_);
    if (closeResult == MMSYSERR_NOERROR) {
        // A successful close guarantees that no queued buffers or callbacks
        // remain, so earlier stop/reset diagnostics are no longer a lifetime
        // hazard. Preserve them for status reporting but allow safe deletion.
        input_ = nullptr;
        if (firstError != MMSYSERR_NOERROR)
            capture_error_.store(int(firstError), std::memory_order_release);
        return MMSYSERR_NOERROR;
    }
    // Keep the handle and object alive. The caller must not destroy this
    // block while WinMM may still deliver callbacks into it.
    return closeResult;
}

int WinmmIqSource::work(int noutputItems,
                        gr_vector_const_void_star&,
                        gr_vector_void_star& outputItems)
{
    auto* outputI = static_cast<float*>(outputItems[0]);
    auto* outputQ = channels_ == 2
                        ? static_cast<float*>(outputItems[1])
                        : nullptr;
    std::unique_lock<std::mutex> lock(queue_mutex_);
    queue_ready_.wait_for(lock, std::chrono::milliseconds(100), [this] {
        return stopping_.load(std::memory_order_acquire) ||
               samples_.size() >= std::size_t(channels_) ||
               capture_error_.load(std::memory_order_acquire) != MMSYSERR_NOERROR;
    });
    if (stopping_.load(std::memory_order_acquire) &&
        samples_.size() < std::size_t(channels_))
        return -1;
    if (capture_error_.load(std::memory_order_acquire) != MMSYSERR_NOERROR &&
        samples_.size() < std::size_t(channels_))
        return -1;
    if (samples_.size() < std::size_t(channels_))
        return 0;

    const int frames = std::min(
        noutputItems, int(samples_.size() / std::size_t(channels_)));
    for (int index = 0; index < frames; ++index) {
        const float left = float(samples_.front()) / 32768.0f;
        samples_.pop_front();
        if (channels_ == 1) {
            outputI[index] = left;
        } else {
            const float right = float(samples_.front()) / 32768.0f;
            samples_.pop_front();
            outputI[index] = swap_iq_ ? right : left;
            if (outputQ)
                outputQ[index] = swap_iq_ ? left : right;
        }
    }
    return frames;
}

std::uint64_t WinmmIqSource::droppedFrames() const noexcept
{
    return dropped_frames_.load(std::memory_order_relaxed);
}

int WinmmIqSource::captureError() const noexcept
{
    return capture_error_.load(std::memory_order_acquire);
}

#endif
