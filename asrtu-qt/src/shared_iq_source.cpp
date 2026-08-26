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
}

std::uint64_t SharedIqSource::writeIndex() const
{
    if (!base_)
        return 0;
#ifdef _WIN32
    MemoryBarrier();
#endif
    return readValue<std::uint64_t>(base_, 24);
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
    if (written < read_index_)
        read_index_ = written;
    if (written - read_index_ > kCapacitySamples)
        read_index_ = written - kCapacitySamples;

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
    read_index_ += count;
    last_sample_time_ns_.store(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count(),
        std::memory_order_relaxed);
    return static_cast<int>(count);
}
