#pragma once

#include <gnuradio/hier_block2.h>

#include <memory>
#include <string>

class WavIqSource final : public gr::hier_block2
{
public:
    using sptr = std::shared_ptr<WavIqSource>;

    static sptr make(const std::string& path, int outputSampleRate,
                     bool repeat, bool throttle);

    WavIqSource(const std::string& path, int outputSampleRate,
                bool repeat, bool throttle);
};
