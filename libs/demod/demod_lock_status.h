#pragma once

#include <atomic>
#include <cstdint>

// One atomic timestamp publishes lock and sample freshness together.
class DemodLockStatus
{
      public:
	static constexpr std::int64_t inputTimeoutMs = 500;
	void update(bool locked, std::int64_t sampleTimeMs)
	{
		last_locked_sample_ms_.store(locked ? sampleTimeMs : 0,
					     std::memory_order_relaxed);
	}
	bool locked(std::int64_t nowMs) const
	{
		const auto last =
		    last_locked_sample_ms_.load(std::memory_order_relaxed);
		return last > 0 && nowMs >= last &&
		       nowMs - last < inputTimeoutMs;
	}

      private:
	std::atomic<std::int64_t> last_locked_sample_ms_{0};
};
