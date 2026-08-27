#include "fec_candidate_sink.h"

#include <gnuradio/io_signature.h>
#include <pmt/pmt.h>

FecCandidateSink::sptr FecCandidateSink::make()
{
    return sptr(new FecCandidateSink());
}

FecCandidateSink::FecCandidateSink()
    : gr::sync_block("asrtu_fec_candidate_sink",
                     gr::io_signature::make(1, 1, sizeof(char)),
                     gr::io_signature::make(0, 0, 0))
{
    message_port_register_out(pmt::intern("out"));
    message_port_register_out(pmt::intern("failed"));
    set_output_multiple(16);
    ccsds_init(&decoder_, 0x1ACFFC1D, 223, this, frameCallback);
    decoder_.cfg_using_m = 1;
    decoder_.cfg_using_convolutional_code = 0;
}

int FecCandidateSink::work(int noutputItems,
                           gr_vector_const_void_star& inputItems,
                           gr_vector_void_star&)
{
    const auto* input = static_cast<const unsigned char*>(inputItems[0]);
    ccsds_rx_proc(&decoder_, const_cast<unsigned char*>(input),
                  unsigned(noutputItems));
    ccsds_pull(&decoder_);
    return noutputItems;
}

void FecCandidateSink::frameCallback(std::uint8_t* data,
                                     std::uint16_t length,
                                     std::int16_t corrected,
                                     void* context)
{
    static_cast<FecCandidateSink*>(context)->publish(data, length, corrected);
}

void FecCandidateSink::publish(std::uint8_t* data,
                               std::uint16_t length,
                               std::int16_t corrected)
{
    auto metadata = pmt::make_dict();
    metadata = pmt::dict_add(metadata, pmt::intern("rs_corrected"),
                             pmt::from_long(corrected));
    const auto payload = pmt::init_u8vector(length, data);
    message_port_pub(pmt::intern(corrected < 0 ? "failed" : "out"),
                     pmt::cons(metadata, payload));
}
