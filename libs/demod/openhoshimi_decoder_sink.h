#pragma once

#include <gnuradio/sync_block.h>
#include "demod_lock_status.h"
#include "snr_sample_buffer.h"

#include <cstdint>
#include <memory>
#include <atomic>
#include <limits>
#include <vector>

class OpenHoshimiDecoderSink final : public gr::sync_block
{
public:
    using sptr = std::shared_ptr<OpenHoshimiDecoderSink>;

    static sptr make(std::shared_ptr<SnrSampleBuffer> buffer);
    ~OpenHoshimiDecoderSink() override;

    int work(int noutput_items,
             gr_vector_const_void_star& input_items,
             gr_vector_void_star& output_items) override;
    bool stop() override;
    bool locked() const;
    std::uint64_t processedSamples() const
    {
	    return processed_samples_.load(std::memory_order_acquire);
    }
    double snr() const
    {
	    return snr_db_.load(std::memory_order_relaxed);
    }

private:
  OpenHoshimiDecoderSink(std::shared_ptr<SnrSampleBuffer> buffer);
  void publishQueuedFrames();

  struct DecoderDeleter {
	  void operator()(void *decoder) const noexcept;
    };
    std::unique_ptr<void, DecoderDeleter> decoder_;
    std::vector<float> interleaved_;
    std::shared_ptr<SnrSampleBuffer> snr_samples_;
    std::atomic<double> snr_db_{std::numeric_limits<double>::quiet_NaN()};
    std::atomic<std::uint64_t> processed_samples_{0};
    DemodLockStatus lock_status_;
    std::int64_t last_input_ms_ = 0;
};
