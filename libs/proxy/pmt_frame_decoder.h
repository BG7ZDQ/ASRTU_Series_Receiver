#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace asrtu
{

constexpr std::size_t kTelemetryFrameBytes = 223;

bool decodePmtTelemetryFrame(const void *serialized, std::size_t size,
			     std::vector<std::uint8_t> *frame,
			     std::string *error = nullptr);

} // namespace asrtu
