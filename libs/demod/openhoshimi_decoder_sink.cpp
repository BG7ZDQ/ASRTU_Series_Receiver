#include "openhoshimi_decoder_sink.h"

extern "C" {
#include "sm_api.h"
}

#include <gnuradio/gr_complex.h>
#include <gnuradio/io_signature.h>
#include <pmt/pmt.h>

#include <array>
#include <stdexcept>

OpenHoshimiDecoderSink::sptr OpenHoshimiDecoderSink::make()
{
    return sptr(new OpenHoshimiDecoderSink());
}

OpenHoshimiDecoderSink::OpenHoshimiDecoderSink()
    : gr::sync_block("openhoshimi_decoder_sink",
                     gr::io_signature::make(1, 1, sizeof(gr_complex)),
                     gr::io_signature::make(0, 0, 0)),
      decoder_(sm_decoder_new())
{
    if (!decoder_)
        throw std::runtime_error("Unable to create OpenHoshimi decoder");
    message_port_register_out(pmt::intern("out"));
}

OpenHoshimiDecoderSink::~OpenHoshimiDecoderSink() = default;

void OpenHoshimiDecoderSink::DecoderDeleter::operator()(void* decoder) const noexcept
{
    sm_decoder_free(static_cast<sm_decoder*>(decoder));
}

int OpenHoshimiDecoderSink::work(int noutput_items,
                                 gr_vector_const_void_star& input_items,
                                 gr_vector_void_star&)
{
    const auto* samples = static_cast<const gr_complex*>(input_items[0]);
    // std::complex<float> is represented by adjacent I/Q floats on all
    // supported GNU Radio targets. Copying also keeps the C ABI independent.
    interleaved_.resize(static_cast<std::size_t>(noutput_items) * 2U);
    for (int i = 0; i < noutput_items; ++i) {
        interleaved_[2U * i] = samples[i].real();
        interleaved_[2U * i + 1U] = samples[i].imag();
    }
    sm_decoder_feed_iq(static_cast<sm_decoder*>(decoder_.get()),
                       interleaved_.data(), static_cast<std::size_t>(noutput_items));
    publishQueuedFrames();
    return noutput_items;
}

bool OpenHoshimiDecoderSink::stop()
{
    sm_decoder_flush(static_cast<sm_decoder*>(decoder_.get()));
    publishQueuedFrames();
    return gr::sync_block::stop();
}

void OpenHoshimiDecoderSink::publishQueuedFrames()
{
    std::array<std::uint8_t, SM_FRAME_LEN> frame{};
    int corrected = 0;
    while (sm_decoder_poll(static_cast<sm_decoder*>(decoder_.get()),
                           frame.data(), &corrected)) {
        auto metadata = pmt::make_dict();
        metadata = pmt::dict_add(metadata, pmt::intern("rs_corrected"),
                                 pmt::from_long(corrected));
        const auto bytes = pmt::init_u8vector(frame.size(), frame.data());
        message_port_pub(pmt::intern("out"), pmt::cons(metadata, bytes));
    }
}
