#include "shared_iq_source.h"

#include <gnuradio/gr_complex.h>
#include <gnuradio/io_signature.h>
#include <gnuradio/sptr_magic.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <thread>

namespace {
constexpr wchar_t kMappingName[] = L"Local\\ASRTU_IQ_BRIDGE_V1";
constexpr std::uint32_t kMagic = 0x42514941;
constexpr std::uint32_t kVersion = 1;
constexpr std::size_t kHeaderBytes = 64;
constexpr std::uint32_t kCapacitySamples = 262144;
constexpr std::uint32_t kComplexBytes = 8;
constexpr std::uint64_t kMaximumLatencySamples = 5760; // 120 ms at 48 ksample/s
constexpr auto kProducerTimeout = std::chrono::milliseconds(1000);
constexpr std::size_t kMappingBytes =
    kHeaderBytes + std::size_t(kCapacitySamples) * kComplexBytes;

template <typename T>
T readValue(const std::uint8_t* base, std::size_t offset)
{
    T value{};
    std::memcpy(&value, base + offset, sizeof(value));
    return value;
}
}

SharedIqSource::sptr SharedIqSource::make()
{
    return gnuradio::make_block_sptr<SharedIqSource>();
}

SharedIqSource::SharedIqSource()
    : gr::sync_block("ASRTU SDRSharp local IQ bridge",
                     gr::io_signature::make(0, 0, 0),
                     gr::io_signature::make(1, 1, sizeof(gr_complex)))
{
}

SharedIqSource::~SharedIqSource()
{
    closeMapping();
}

bool SharedIqSource::start()
{
    openMapping();
    return true;
}

bool SharedIqSource::stop()
{
    closeMapping();
    return true;
}

bool SharedIqSource::hasRecentSamples(std::chrono::milliseconds timeout) const
{
    const auto last = last_sample_time_ns_.load(std::memory_order_relaxed);
    if (last == 0)
        return false;
    const auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    return now - last <=
           std::chrono::duration_cast<std::chrono::nanoseconds>(timeout).count();
}

bool SharedIqSource::openMapping()
{
#ifdef _WIN32
    if (base_)
        return true;
    mapping_ = OpenFileMappingW(FILE_MAP_READ, FALSE, kMappingName);
    if (!mapping_)
        return false;
    base_ = static_cast<const std::uint8_t*>(
        MapViewOfFile(mapping_, FILE_MAP_READ, 0, 0, kMappingBytes));
    if (!base_) {
        CloseHandle(mapping_);
        mapping_ = nullptr;
        return false;
    }
    const double sampleRate = readValue<double>(base_, 8);
    if (readValue<std::uint32_t>(base_, 0) != kMagic ||
        readValue<std::uint32_t>(base_, 4) != kVersion ||
        readValue<std::uint32_t>(base_, 16) != kCapacitySamples ||
        readValue<std::uint32_t>(base_, 20) != kComplexBytes ||
        std::abs(sampleRate - 48000.0) > 1.0) {
        closeMapping();
        return false;
    }
    read_index_ = writeIndex();
    return true;
#else
    return false;
#endif
}

void SharedIqSource::closeMapping()
{
#ifdef _WIN32
    if (base_)
        UnmapViewOfFile(base_);
    base_ = nullptr;
    if (mapping_)
        CloseHandle(mapping_);
    mapping_ = nullptr;
#else
    base_ = nullptr;
#endif
    read_index_ = 0;
    producer_was_active_ = false;
    last_sample_time_ns_.store(0, std::memory_order_relaxed);
}

std::uint64_t SharedIqSource::writeIndex() const
{
    if (!base_)
        return 0;
#ifdef _WIN32
    const auto* address = reinterpret_cast<const volatile LONG64*>(base_ + 24);
    // The view is deliberately read-only. InterlockedCompareExchange64 is a
    // read-modify-write operation even when both operands are zero and faults
    // on FILE_MAP_READ mappings. The decoder is x64 and this field is aligned,
    // so a volatile 64-bit load is atomic; the barrier supplies acquire
    // ordering for the sample data published before the index.
    const LONG64 value = *address;
    MemoryBarrier();
    return static_cast<std::uint64_t>(value);
#else
    return readValue<std::uint64_t>(base_, 24);
#endif
}

bool SharedIqSource::producerActive() const
{
#ifdef _WIN32
    if (!base_)
        return false;
    const auto* enabledAddress =
        reinterpret_cast<const volatile LONG*>(base_ + 40);
    const LONG enabled = *enabledAddress;
    MemoryBarrier();
    if (enabled == 0)
        return false;
    const auto* timestampAddress =
        reinterpret_cast<const volatile LONG64*>(base_ + 48);
    const LONGLONG writtenAt = *timestampAddress;
    MemoryBarrier();
    LARGE_INTEGER now{};
    LARGE_INTEGER frequency{};
    if (writtenAt <= 0 || !QueryPerformanceCounter(&now) ||
        !QueryPerformanceFrequency(&frequency) || frequency.QuadPart <= 0)
        return false;
    const auto ageTicks = std::max<LONGLONG>(0, now.QuadPart - writtenAt);
    const auto ageMs = ageTicks * 1000 / frequency.QuadPart;
    return ageMs <= kProducerTimeout.count();
#else
    return false;
#endif
}

int SharedIqSource::work(int noutputItems,
                         gr_vector_const_void_star&,
                         gr_vector_void_star& outputItems)
{
    if (!base_ && !openMapping()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return 0;
    }

    const std::uint64_t written = writeIndex();
    const bool active = producerActive();
    if (!active) {
        producer_was_active_ = false;
        last_sample_time_ns_.store(0, std::memory_order_relaxed);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return 0;
    }
    if (!producer_was_active_) {
        read_index_ = written;
        producer_was_active_ = true;
    }
    if (written < read_index_)
        read_index_ = written;
    const std::uint64_t backlog = written - read_index_;
    if (backlog > kMaximumLatencySamples) {
        const auto skipped = backlog - kMaximumLatencySamples;
        read_index_ += skipped;
        dropped_samples_.fetch_add(skipped, std::memory_order_relaxed);
    }

    const auto available = written - read_index_;
    if (available == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        return 0;
    }

    const auto count = static_cast<std::uint32_t>(
        std::min<std::uint64_t>(available, std::uint64_t(noutputItems)));
    auto* output = static_cast<gr_complex*>(outputItems[0]);
    const auto ringOffset = static_cast<std::uint32_t>(
        read_index_ % kCapacitySamples);
    const auto first = std::min(count, kCapacitySamples - ringOffset);
    const auto* data = base_ + kHeaderBytes;
    std::memcpy(output, data + std::size_t(ringOffset) * kComplexBytes,
                std::size_t(first) * kComplexBytes);
    if (first < count) {
        std::memcpy(output + first, data,
                    std::size_t(count - first) * kComplexBytes);
    }
    // If the producer managed to lap this read despite the low-latency skip,
    // discard the torn block and resume from a recent position next time.
    const std::uint64_t afterCopy = writeIndex();
    if (afterCopy < read_index_) {
        read_index_ = afterCopy;
        producer_was_active_ = false;
        return 0;
    }
    if (afterCopy - read_index_ > kCapacitySamples) {
        const auto skipped = afterCopy - read_index_;
        read_index_ = afterCopy;
        dropped_samples_.fetch_add(skipped, std::memory_order_relaxed);
        return 0;
    }
    read_index_ += count;
    last_sample_time_ns_.store(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count(),
        std::memory_order_relaxed);
    return static_cast<int>(count);
}

std::uint64_t SharedIqSource::droppedSamples() const noexcept
{
    return dropped_samples_.load(std::memory_order_relaxed);
}
