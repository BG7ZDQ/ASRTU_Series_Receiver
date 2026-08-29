#include "pmt_frame_decoder.h"

#include <pmt/pmt.h>

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

int main()
{
	std::vector<std::uint8_t> expected(asrtu::kTelemetryFrameBytes);
	for (std::size_t i = 0; i < expected.size(); ++i)
		expected[i] = static_cast<std::uint8_t>(i);

	const auto vector =
	    pmt::init_u8vector(expected.size(), expected.data());
	const auto metadata = pmt::dict_add(
	    pmt::make_dict(), pmt::intern("source"), pmt::intern("test"));
	std::vector<std::uint8_t> decoded;
	std::string error;
	const std::string pair =
	    pmt::serialize_str(pmt::cons(metadata, vector));
	if (!asrtu::decodePmtTelemetryFrame(pair.data(), pair.size(), &decoded,
					    &error) ||
	    decoded != expected) {
		std::cerr << "Failed to decode a metadata-bearing PDU: "
			  << error << '\n';
		return 1;
	}

	const std::string bare = pmt::serialize_str(vector);
	if (!asrtu::decodePmtTelemetryFrame(bare.data(), bare.size(), &decoded,
					    &error) ||
	    decoded != expected) {
		std::cerr << "Failed to decode a bare u8vector: " << error
			  << '\n';
		return 1;
	}

	const auto shortVector =
	    pmt::init_u8vector(expected.size() - 1, expected.data());
	const std::string shortMessage =
	    pmt::serialize_str(pmt::cons(pmt::PMT_NIL, shortVector));
	if (asrtu::decodePmtTelemetryFrame(
		shortMessage.data(), shortMessage.size(), &decoded, &error) ||
	    !decoded.empty()) {
		std::cerr << "Accepted a non-223-byte telemetry PDU\n";
		return 1;
	}

	const std::string malformed("not-pmt");
	if (asrtu::decodePmtTelemetryFrame(malformed.data(), malformed.size(),
					   &decoded, &error)) {
		std::cerr << "Accepted malformed PMT input\n";
		return 1;
	}
	return 0;
}
