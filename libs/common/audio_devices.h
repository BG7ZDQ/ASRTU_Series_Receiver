#pragma once

#include <string>
#include <vector>

namespace asrtu
{

struct AudioInputDeviceInfo {
	std::string name;
	int channels = 0;
};

// macOS only.  Enumerates the CoreAudio devices that expose input channels.
//
// The launcher populates its sound-card list from this, and the DSP resolves
// --audio-device by indexing the same list, so the index shown to the user and
// the index the flowgraph resolves cannot drift apart.  Windows uses waveIn
// ids and Linux uses ALSA "plughw:N,0" strings, so neither needs this.
std::vector<AudioInputDeviceInfo> macInputDevices();

} // namespace asrtu
