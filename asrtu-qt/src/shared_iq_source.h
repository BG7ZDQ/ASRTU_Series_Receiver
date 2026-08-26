#pragma once

#include <gnuradio/sync_block.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>

#ifdef _WIN32
#include <windows.h>
#endif

class SharedIqSource final : public gr::sync_block
{
public:
    using sptr = std::shared_ptr<SharedIqSource>;

    static sptr make();
    SharedIqSource();
    ~SharedIqSource() override;

    bool start() override;
    bool stop() override;
    bool hasRecentSamples(std::chrono::milliseconds timeout) const;
    int work(int noutputItems,
             gr_vector_const_void_star& inputItems,
             gr_vector_void_star& outputItems) override;

private:
    bool openMapping();
    void closeMapping();
    std::uint64_t writeIndex() const;

#ifdef _WIN32
    HANDLE mapping_ = nullptr;
#endif
    const std::uint8_t* base_ = nullptr;
    std::uint64_t read_index_ = 0;
    std::atomic<std::int64_t> last_sample_time_ns_{0};
};
