#include "shared_iq_source.h"

#include <gnuradio/gr_complex.h>
#include <windows.h>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

namespace {
constexpr std::size_t kHeaderBytes = 64;
constexpr std::uint32_t kCapacity = 262144;
constexpr std::size_t kBytes = kHeaderBytes + std::size_t(kCapacity) * 8;

template <typename T>
void put(std::uint8_t* base, std::size_t offset, const T& value)
{
    std::memcpy(base + offset, &value, sizeof(value));
}
}

int main()
{
    HANDLE mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr,
                                        PAGE_READWRITE, 0, DWORD(kBytes),
                                        L"Local\\ASRTU_IQ_BRIDGE_V1");
    if (!mapping)
        return 1;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mapping);
        return 0; // Never disturb a receiver that is currently on the air.
    }
    auto* base = static_cast<std::uint8_t*>(
        MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, kBytes));
    if (!base) {
        CloseHandle(mapping);
        return 1;
    }
    std::memset(base, 0, kBytes);
    put(base, 0, std::uint32_t(0x42514941));
    put(base, 4, std::uint32_t(1));
    put(base, 8, double(48000.0));
    put(base, 16, kCapacity);
    put(base, 20, std::uint32_t(8));
    InterlockedExchange(reinterpret_cast<volatile LONG*>(base + 40), 1);
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);
    InterlockedExchange64(reinterpret_cast<volatile LONG64*>(base + 48),
                          now.QuadPart);

    auto source = SharedIqSource::make();
    source->start();
    std::vector<gr_complex> output(1024);
    gr_vector_const_void_star inputs;
    gr_vector_void_star outputs{output.data()};
    // A newly attached consumer intentionally starts at the current write
    // index and must not block when nothing has been published yet.
    if (source->work(int(output.size()), inputs, outputs) != 0) {
        std::cerr << "shared IQ source returned data before any burst\n";
        source->stop();
        UnmapViewOfFile(base);
        CloseHandle(mapping);
        return 1;
    }
    auto* samples = reinterpret_cast<gr_complex*>(base + kHeaderBytes);

    // SDR# publishes the post-decimation stream in short callbacks. The source
    // must forward each burst immediately without blocking the flowgraph.
    for (std::uint64_t end = 256; end <= 1024; end += 256) {
        const auto begin = end - 256;
        for (std::uint64_t index = begin; index < end; ++index)
            samples[index] = gr_complex(float(index), -float(index));
        LARGE_INTEGER publishedAt{};
        QueryPerformanceCounter(&publishedAt);
        InterlockedExchange64(reinterpret_cast<volatile LONG64*>(base + 48),
                              publishedAt.QuadPart);
        InterlockedExchange64(reinterpret_cast<volatile LONG64*>(base + 24),
                              static_cast<LONG64>(end));
        const int produced = source->work(int(output.size()), inputs, outputs);
        if (produced != 256 || output.front() != samples[begin] ||
            output[produced - 1] != samples[end - 1]) {
            std::cerr << "shared IQ source dropped or misaligned a burst: "
                      << produced << " samples, first=" << output.front()
                      << " expected=" << samples[begin]
                      << ", last=" << output[produced - 1]
                      << " expected=" << samples[end - 1]
                      << ", writeIndex=" << end << '\n';
            source->stop();
            UnmapViewOfFile(base);
            CloseHandle(mapping);
            return 1;
        }
    }

    // A stalled consumer accumulates backlog; the source skips data older than
    // its 120 ms latency bound instead of emitting a stale block.
    for (std::uint64_t index = 1024; index < 11024; ++index)
        samples[index] = gr_complex(float(index), -float(index));
    QueryPerformanceCounter(&now);
    InterlockedExchange64(reinterpret_cast<volatile LONG64*>(base + 48),
                          now.QuadPart);
    InterlockedExchange64(reinterpret_cast<volatile LONG64*>(base + 24), 11024);

    const int produced = source->work(int(output.size()), inputs, outputs);
    // read_index=1024, backlog=10000 > 5760 -> skip 4240, resume at 5264.
    const bool ok = produced == int(output.size()) &&
                    source->droppedSamples() == 4240 &&
                    output.front() == samples[5264] &&
                    output[produced - 1] == samples[6287];
    source->stop();
    UnmapViewOfFile(base);
    CloseHandle(mapping);
    if (!ok) {
        std::cerr << "shared IQ source did not enforce its latency bound: "
                  << "produced=" << produced
                  << " dropped=" << source->droppedSamples()
                  << " first=" << output.front() << " expected=" << samples[5264]
                  << " last=" << output[produced - 1]
                  << " expected=" << samples[6287]
                  << '\n';
        return 1;
    }
    return 0;
}