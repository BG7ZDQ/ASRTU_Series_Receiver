#pragma once

#include "snr_sample_buffer.h"
#include <gnuradio/digital/mpsk_snr_est.h>
#include <gnuradio/io_signature.h>
#include <gnuradio/sync_block.h>
#include <algorithm>
#include <atomic>
#include <memory>

// Same GNU Radio SVR kernel, alpha and stream as the original probe, but
// capture every audio interval inside work(), including large replay batches.
class SvrSnrSink final : public gr::sync_block
{
      public:
	static std::shared_ptr<SvrSnrSink>
	make(std::shared_ptr<SnrSampleBuffer> buffer)
	{
		return gnuradio::make_block_sptr<SvrSnrSink>(std::move(buffer));
	}
	explicit SvrSnrSink(std::shared_ptr<SnrSampleBuffer> buffer)
	    : gr::sync_block("asrtu_svr_snr_sink",
			     gr::io_signature::make(1, 1, sizeof(gr_complex)),
			     gr::io_signature::make(0, 0, 0)),
	      buffer_(std::move(buffer))
	{
		set_history(
		    2); // SVR reads adjacent symbols, as in GNU Radio's probe.
	}
	int work(int noutput, gr_vector_const_void_star &inputs,
		 gr_vector_void_star &) override
	{
		const auto *input = static_cast<const gr_complex *>(inputs[0]);
		int offset = 0;
		while (offset < noutput) {
			const auto remaining =
			    SnrSampleBuffer::symbolInterval -
			    samples_ % SnrSampleBuffer::symbolInterval;
			const int count =
			    std::min(noutput - offset, int(remaining));
			estimator_.update(count, input + offset);
			samples_ += std::uint64_t(count);
			offset += count;
			if (samples_ % SnrSampleBuffer::symbolInterval == 0)
				buffer_->svr(
				    samples_ / SnrSampleBuffer::symbolInterval,
				    estimator_.snr());
		}
		latest_.store(estimator_.snr(), std::memory_order_relaxed);
		return noutput;
	}
	double snr() const
	{
		return latest_.load(std::memory_order_relaxed);
	}

      private:
	std::shared_ptr<SnrSampleBuffer> buffer_;
	gr::digital::mpsk_snr_est_svr estimator_{0.001};
	std::uint64_t samples_ = 0;
	std::atomic<double> latest_{std::numeric_limits<double>::quiet_NaN()};
};
