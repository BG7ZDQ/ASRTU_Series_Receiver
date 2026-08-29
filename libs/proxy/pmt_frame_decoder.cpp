#include "pmt_frame_decoder.h"

#include <pmt/pmt.h>

#include <exception>
#include <string>

namespace asrtu
{
namespace
{
bool fail(const char *message, std::string *error)
{
	if (error)
		*error = message;
	return false;
}
} // namespace

bool decodePmtTelemetryFrame(const void *serialized, std::size_t size,
			     std::vector<std::uint8_t> *frame,
			     std::string *error)
{
	if (!frame)
		return fail("missing output frame", error);
	frame->clear();
	if (!serialized || size == 0)
		return fail("empty PMT message", error);

	try {
		const auto message = pmt::deserialize_str(
		    std::string(static_cast<const char *>(serialized), size));
		const auto data =
		    pmt::is_pair(message) ? pmt::cdr(message) : message;
		if (!pmt::is_u8vector(data))
			return fail("PMT message does not contain a u8vector",
				    error);

		std::size_t length = 0;
		const auto *bytes = pmt::u8vector_elements(data, length);
		if (length != kTelemetryFrameBytes)
			return fail(
			    "PMT message is not a 223-byte telemetry frame",
			    error);
		frame->assign(bytes, bytes + length);
		if (error)
			error->clear();
		return true;
	} catch (const std::exception &exception) {
		if (error)
			*error = std::string("invalid PMT message: ") +
				 exception.what();
		return false;
	} catch (...) {
		return fail("invalid PMT message", error);
	}
}

} // namespace asrtu
