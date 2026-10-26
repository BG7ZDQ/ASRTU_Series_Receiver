#pragma once

#include <algorithm>
#include <cmath>

struct SnrScale {
	double minimum = -5.0;
	double maximum = 20.0;
	double step = 5.0;
};

inline SnrScale snrScale(double minimum, double maximum)
{
	minimum = std::min(-5.0, minimum);
	maximum = std::max(20.0, maximum);
	// Integer 1/2/5 steps, at most six grid intervals, always including
	// zero.
	for (double decade = 1.0;; decade *= 10.0) {
		for (const double factor : {1.0, 2.0, 5.0}) {
			const double step = factor * decade;
			if (step < 5.0)
				continue;
			const double low = std::floor(minimum / step) * step;
			const double high = std::ceil(maximum / step) * step;
			if ((high - low) / step <= 6.0)
				return {low, high, step};
		}
	}
}
