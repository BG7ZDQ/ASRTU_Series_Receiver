#pragma once

#include <cstdint>
#include <limits>
#include <map>
#include <mutex>
#include <vector>

struct SnrSample {
	double seconds;
	double value;
	bool synchronized;
};

// Both DSP paths publish every 100 ms of audio. Joining by interval keeps
// algorithm selection tied to historical loop lock, not the current UI state.
class SnrSampleBuffer final
{
      public:
	static constexpr std::uint64_t inputInterval = 4800; // 48 kHz
	static constexpr std::uint64_t symbolInterval = 960; // 9.6 kbaud
	static constexpr double intervalSeconds = 0.1;
	static constexpr std::size_t capacity = 1200; // 120 s; plot shows 60 s

	explicit SnrSampleBuffer(bool useDecoder = true)
	    : use_decoder_(useDecoder)
	{
	}

	void decoder(std::uint64_t tick, double value, bool locked)
	{
		std::lock_guard<std::mutex> guard(mutex_);
		if (tick <= emitted_)
			return;
		auto &entry = entries_[tick];
		entry.decoder_ready = true;
		entry.locked = locked;
		entry.synchronized = value;
		trim();
	}
	void svr(std::uint64_t tick, double value)
	{
		std::lock_guard<std::mutex> guard(mutex_);
		if (tick <= emitted_)
			return;
		auto &entry = entries_[tick];
		entry.svr_ready = true;
		entry.svr = value;
		trim();
	}
	std::vector<SnrSample> take()
	{
		std::lock_guard<std::mutex> guard(mutex_);
		std::vector<SnrSample> result;
		while (!entries_.empty()) {
			const auto &entry = entries_.begin()->second;
			if (use_decoder_ && !entry.decoder_ready)
				break;
			const bool synchronized = use_decoder_ && entry.locked;
			if (!synchronized && !entry.svr_ready)
				break;
			emitted_ = entries_.begin()->first;
			result.push_back(
			    {double(emitted_) * intervalSeconds,
			     synchronized ? entry.synchronized : entry.svr,
			     synchronized});
			entries_.erase(entries_.begin());
		}
		return result;
	}

      private:
	struct Entry {
		double synchronized = std::numeric_limits<double>::quiet_NaN();
		double svr = std::numeric_limits<double>::quiet_NaN();
		bool decoder_ready = false;
		bool svr_ready = false;
		bool locked = false;
	};
	void trim()
	{
		while (entries_.size() > capacity) {
			emitted_ = entries_.begin()->first;
			entries_.erase(entries_.begin());
		}
	}
	const bool use_decoder_;
	std::mutex mutex_;
	std::map<std::uint64_t, Entry> entries_;
	std::uint64_t emitted_ = 0;
};
