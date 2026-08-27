#pragma once

#include <gnuradio/sync_block.h>

#include <cstdint>

extern "C" {
#include "ccsds.h"
}

// CCSDS deframer/FEC sink that keeps successful and failed RS candidates on
// separate message ports.  gr-lilacsat's pass_all mode discards byte_corr,
// which makes safe local-only SSDV fallback arbitration impossible.
class FecCandidateSink final : public gr::sync_block
{
public:
    using sptr = std::shared_ptr<FecCandidateSink>;
    static sptr make();

    int work(int noutputItems,
             gr_vector_const_void_star& inputItems,
             gr_vector_void_star& outputItems) override;

private:
    FecCandidateSink();
    static void frameCallback(std::uint8_t* data, std::uint16_t length,
                              std::int16_t corrected, void* context);
    void publish(std::uint8_t* data, std::uint16_t length,
                 std::int16_t corrected);

    Ccsds decoder_{};
};
