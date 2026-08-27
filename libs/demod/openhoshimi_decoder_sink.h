#pragma once

#include <gnuradio/sync_block.h>

#include <memory>
#include <vector>

class OpenHoshimiDecoderSink final : public gr::sync_block
{
public:
    using sptr = std::shared_ptr<OpenHoshimiDecoderSink>;

    static sptr make();
    ~OpenHoshimiDecoderSink() override;

    int work(int noutput_items,
             gr_vector_const_void_star& input_items,
             gr_vector_void_star& output_items) override;
    bool stop() override;

private:
    OpenHoshimiDecoderSink();
    void publishQueuedFrames();

    struct DecoderDeleter {
        void operator()(void* decoder) const noexcept;
    };
    std::unique_ptr<void, DecoderDeleter> decoder_;
    std::vector<float> interleaved_;
};
