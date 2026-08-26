#pragma once

#include <gnuradio/block.h>
#include <pmt/pmt.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class FrameMonitor final : public gr::block
{
public:
    using sptr = std::shared_ptr<FrameMonitor>;
    using Callback = std::function<void(const std::string&)>;
    using PayloadCallback = std::function<void(const std::vector<std::uint8_t>&)>;

    static sptr make(Callback callback, PayloadCallback payloadCallback = {});
    std::uint64_t frameCount() const noexcept;
    double secondsSinceFrame() const noexcept;
    int general_work(int noutput_items,
                     gr_vector_int& ninput_items,
                     gr_vector_const_void_star& input_items,
                     gr_vector_void_star& output_items) override;

private:
    FrameMonitor(Callback callback, PayloadCallback payloadCallback);
    void handle(const pmt::pmt_t& message);
    static std::string describePdu(const pmt::pmt_t& message);

    Callback callback_;
    PayloadCallback payload_callback_;
    std::atomic<std::uint64_t> frame_count_{0};
    std::atomic<std::int64_t> last_frame_ms_{0};
};
