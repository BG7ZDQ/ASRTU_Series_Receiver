#include "async_wav_recorder_sink.h"

#include <gnuradio/io_signature.h>
#include <gnuradio/sptr_magic.h>

#include <QDir>
#include <QFileInfo>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace {
void put16(unsigned char* output, std::uint16_t value)
{
    output[0] = static_cast<unsigned char>(value & 0xffU);
    output[1] = static_cast<unsigned char>((value >> 8U) & 0xffU);
}

void put32(unsigned char* output, std::uint32_t value)
{
    output[0] = static_cast<unsigned char>(value & 0xffU);
    output[1] = static_cast<unsigned char>((value >> 8U) & 0xffU);
    output[2] = static_cast<unsigned char>((value >> 16U) & 0xffU);
    output[3] = static_cast<unsigned char>((value >> 24U) & 0xffU);
}
}

AsyncWavRecorderSink::sptr AsyncWavRecorderSink::make(
    const QString& path, int channels, int sampleRate,
    std::uint32_t maximumDataBytes)
{
    return gnuradio::make_block_sptr<AsyncWavRecorderSink>(
        path, channels, sampleRate, maximumDataBytes);
}

AsyncWavRecorderSink::AsyncWavRecorderSink(
    const QString& path, int channels, int sampleRate,
    std::uint32_t maximumDataBytes)
    : gr::sync_block("asrtu_async_wav_recorder",
                     gr::io_signature::make(channels, channels, sizeof(float)),
                     gr::io_signature::make(0, 0, 0)),
      original_path_(path), channels_(channels), sample_rate_(sampleRate)
{
    if (channels < 1 || channels > 2 || sampleRate <= 0)
        throw std::invalid_argument("invalid WAV recorder format");
    const std::uint32_t frameBytes = std::uint32_t(channels_ * 2);
    maximum_data_bytes_ = maximumDataBytes - maximumDataBytes % frameBytes;
    if (maximum_data_bytes_ < frameBytes)
        throw std::invalid_argument("WAV segment limit is smaller than one frame");
    if (!openSegment())
        throw std::runtime_error("Unable to open asynchronous WAV recording");
    if (!writeHeader(0)) {
        finalizeSegment();
        throw std::runtime_error("Unable to initialize WAV recording");
    }
    writer_ = std::thread([this] { writerLoop(); });
}

AsyncWavRecorderSink::~AsyncWavRecorderSink()
{
    finalize();
}

bool AsyncWavRecorderSink::stop()
{
    finalize();
    return true;
}

int AsyncWavRecorderSink::work(int noutputItems,
                               gr_vector_const_void_star& inputItems,
                               gr_vector_void_star&)
{
    if (noutputItems <= 0 || stopping_.load(std::memory_order_acquire))
        return noutputItems;
    std::vector<std::int16_t> interleaved(
        std::size_t(noutputItems) * std::size_t(channels_));
    for (int frame = 0; frame < noutputItems; ++frame) {
        for (int channel = 0; channel < channels_; ++channel) {
            const auto* input = static_cast<const float*>(inputItems[channel]);
            const float finite = std::isfinite(input[frame]) ? input[frame] : 0.0f;
            const float limited = std::clamp(finite, -1.0f, 0.9999695f);
            interleaved[std::size_t(frame) * std::size_t(channels_) +
                        std::size_t(channel)] =
                static_cast<std::int16_t>(std::lrint(limited * 32768.0f));
        }
    }

    std::lock_guard<std::mutex> lock(queue_mutex_);
    std::size_t frames = std::size_t(noutputItems);
    if (frames > kMaximumQueuedFrames) {
        const std::size_t keepSamples = kMaximumQueuedFrames * std::size_t(channels_);
        interleaved.erase(interleaved.begin(),
                          interleaved.end() - std::ptrdiff_t(keepSamples));
        dropped_frames_.fetch_add(frames - kMaximumQueuedFrames,
                                  std::memory_order_relaxed);
        frames = kMaximumQueuedFrames;
    }
    while (!chunks_.empty() && queued_frames_ + frames > kMaximumQueuedFrames) {
        const std::size_t removed =
            chunks_.front().size() / std::size_t(channels_);
        queued_frames_ -= removed;
        dropped_frames_.fetch_add(removed, std::memory_order_relaxed);
        chunks_.pop_front();
    }
    chunks_.push_back(std::move(interleaved));
    queued_frames_ += frames;
    queue_ready_.notify_one();
    return noutputItems;
}

void AsyncWavRecorderSink::writerLoop()
{
    while (true) {
        std::vector<std::int16_t> chunk;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_ready_.wait(lock, [this] {
                return stopping_.load(std::memory_order_acquire) || !chunks_.empty();
            });
            if (chunks_.empty()) {
                if (stopping_.load(std::memory_order_acquire))
                    break;
                continue;
            }
            chunk = std::move(chunks_.front());
            chunks_.pop_front();
            queued_frames_ -= chunk.size() / std::size_t(channels_);
        }
        std::size_t sampleOffset = 0;
        const std::uint32_t frameBytes = std::uint32_t(channels_ * 2);
        while (sampleOffset < chunk.size() &&
               !write_failed_.load(std::memory_order_relaxed)) {
            if (segment_data_bytes_ == maximum_data_bytes_) {
                if (!finalizeSegment()) {
                    write_failed_.store(true, std::memory_order_release);
                    break;
                }
                ++segment_index_;
                if (!openSegment() || !writeHeader(0)) {
                    write_failed_.store(true, std::memory_order_release);
                    break;
                }
            }
            const std::size_t framesAvailable =
                (chunk.size() - sampleOffset) / std::size_t(channels_);
            const std::size_t framesCapacity =
                (maximum_data_bytes_ - segment_data_bytes_) / frameBytes;
            const std::size_t frames = std::min(framesAvailable, framesCapacity);
            const std::size_t samples = frames * std::size_t(channels_);
            const std::size_t written = std::fwrite(
                chunk.data() + sampleOffset, sizeof(std::int16_t), samples, file_);
            segment_data_bytes_ += static_cast<std::uint32_t>(
                written * sizeof(std::int16_t));
            if (written != samples) {
                write_failed_.store(true, std::memory_order_release);
                break;
            }
            sampleOffset += samples;
        }
    }
}

void AsyncWavRecorderSink::finalize() noexcept
{
    if (finalized_.exchange(true, std::memory_order_acq_rel))
        return;
    stopping_.store(true, std::memory_order_release);
    queue_ready_.notify_all();
    if (writer_.joinable())
        writer_.join();
    if (!finalizeSegment())
        write_failed_.store(true, std::memory_order_release);
}

QString AsyncWavRecorderSink::segmentPath() const
{
    if (segment_index_ == 1)
        return original_path_;
    const QFileInfo info(original_path_);
    const QString suffix = info.suffix().isEmpty()
                               ? QStringLiteral("wav")
                               : info.suffix();
    return QDir(info.absolutePath()).filePath(
        QStringLiteral("%1_part%2.%3")
            .arg(info.completeBaseName())
            .arg(segment_index_, 2, 10, QLatin1Char('0'))
            .arg(suffix));
}

bool AsyncWavRecorderSink::openSegment() noexcept
{
    segment_data_bytes_ = 0;
    const QString path = segmentPath();
#ifdef _WIN32
    if (_wfopen_s(&file_, reinterpret_cast<const wchar_t*>(path.utf16()), L"w+b") != 0)
        file_ = nullptr;
#else
    file_ = std::fopen(path.toUtf8().constData(), "w+b");
#endif
    return file_ != nullptr;
}

bool AsyncWavRecorderSink::finalizeSegment() noexcept
{
    if (!file_)
        return true;
    const bool headerOk = writeHeader(segment_data_bytes_);
    const bool flushOk = std::fflush(file_) == 0;
    const bool closeOk = std::fclose(file_) == 0;
    file_ = nullptr;
    return headerOk && flushOk && closeOk;
}

bool AsyncWavRecorderSink::writeHeader(std::uint32_t dataBytes) noexcept
{
    if (!file_ || std::fseek(file_, 0, SEEK_SET) != 0)
        return false;
    unsigned char header[44]{};
    std::memcpy(header, "RIFF", 4);
    put32(header + 4, 36U + dataBytes);
    std::memcpy(header + 8, "WAVEfmt ", 8);
    put32(header + 16, 16);
    put16(header + 20, 1);
    put16(header + 22, std::uint16_t(channels_));
    put32(header + 24, std::uint32_t(sample_rate_));
    put32(header + 28, std::uint32_t(sample_rate_ * channels_ * 2));
    put16(header + 32, std::uint16_t(channels_ * 2));
    put16(header + 34, 16);
    std::memcpy(header + 36, "data", 4);
    put32(header + 40, dataBytes);
    const bool ok = std::fwrite(header, 1, sizeof(header), file_) == sizeof(header);
    if (ok)
        std::fseek(file_, 0, SEEK_END);
    return ok;
}

std::uint64_t AsyncWavRecorderSink::droppedFrames() const noexcept
{
    return dropped_frames_.load(std::memory_order_relaxed);
}

bool AsyncWavRecorderSink::writeFailed() const noexcept
{
    return write_failed_.load(std::memory_order_acquire);
}
