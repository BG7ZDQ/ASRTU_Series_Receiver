#pragma once

#include <cmath>
#include <deque>

// Timestamps are decoded audio time, not UI refresh ticks or wall time.
class SnrHistory final
{
      public:
	struct Point {
		double seconds;
		double value;
	};
	static constexpr double windowSeconds = 60.0;

	bool add(double value, double seconds)
	{
		if (!std::isfinite(value) || std::abs(value) >= 1000.0 ||
		    !std::isfinite(seconds) || seconds <= 0.0)
			return false;
		if (!points_.empty()) {
			if (seconds == points_.back().seconds)
				return false; // No new samples: freeze,
					      // including EOF.
			if (seconds < points_.back().seconds)
				clear(); // New input stream or replay
					 // restarted.
		}
		points_.push_back({seconds, value});
		while (points_.front().seconds < seconds - windowSeconds)
			points_.pop_front();
		return true;
	}

	const std::deque<Point> &points() const
	{
		return points_;
	}
	void clear()
	{
		points_.clear();
	}
	double position(const Point &point) const
	{
		return 1.0 -
		       (points_.back().seconds - point.seconds) / windowSeconds;
	}

      private:
	std::deque<Point> points_;
};
