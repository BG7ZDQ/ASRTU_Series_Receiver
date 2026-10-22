#include "audio_devices.h"

#ifdef __APPLE__

#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>

// kAudioObjectPropertyElementMain replaced ...Master in the macOS 12 SDK.  It
// is an enum constant, not a macro, so the availability version has to be
// tested instead of #ifndef.  Both spell the same value (0), so older SDKs keep
// building while newer ones avoid the deprecation warning.
#if !defined(MAC_OS_VERSION_12_0) ||                                           \
    (MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_VERSION_12_0)
#define kAudioObjectPropertyElementMain kAudioObjectPropertyElementMaster
#endif

namespace
{

int inputChannelCount(AudioDeviceID deviceId)
{
	AudioObjectPropertyAddress address{
	    kAudioDevicePropertyStreamConfiguration,
	    kAudioDevicePropertyScopeInput, kAudioObjectPropertyElementMain};
	UInt32 size = 0;
	if (AudioObjectGetPropertyDataSize(deviceId, &address, 0, nullptr,
					   &size) != noErr ||
	    size == 0)
		return 0;

	std::vector<char> storage(size);
	auto *buffers = reinterpret_cast<AudioBufferList *>(storage.data());
	if (AudioObjectGetPropertyData(deviceId, &address, 0, nullptr, &size,
				       buffers) != noErr)
		return 0;

	int channels = 0;
	for (UInt32 i = 0; i < buffers->mNumberBuffers; ++i)
		channels += int(buffers->mBuffers[i].mNumberChannels);
	return channels;
}

std::string deviceName(AudioDeviceID deviceId)
{
	AudioObjectPropertyAddress address{kAudioObjectPropertyName,
					   kAudioObjectPropertyScopeGlobal,
					   kAudioObjectPropertyElementMain};
	CFStringRef name = nullptr;
	UInt32 size = sizeof(name);
	if (AudioObjectGetPropertyData(deviceId, &address, 0, nullptr, &size,
				       &name) != noErr ||
	    name == nullptr)
		return std::string();

	// CFStringGetCString needs a byte budget, not a character count.
	const CFIndex length = CFStringGetLength(name);
	const CFIndex capacity =
	    CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) +
	    1;
	std::vector<char> buffer(size_t(capacity), '\0');
	const bool converted = CFStringGetCString(name, buffer.data(), capacity,
						  kCFStringEncodingUTF8);
	CFRelease(name);
	return converted ? std::string(buffer.data()) : std::string();
}

} // namespace

namespace asrtu
{

int macDefaultInputChannels()
{
	AudioObjectPropertyAddress address{
	    kAudioHardwarePropertyDefaultInputDevice,
	    kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain};
	AudioDeviceID device = kAudioObjectUnknown;
	UInt32 size = sizeof(device);
	if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0,
				       nullptr, &size, &device) != noErr)
		return 0;
	return inputChannelCount(device);
}

std::vector<AudioInputDeviceInfo> macInputDevices()
{
	AudioObjectPropertyAddress address{kAudioHardwarePropertyDevices,
					   kAudioObjectPropertyScopeGlobal,
					   kAudioObjectPropertyElementMain};
	UInt32 size = 0;
	if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &address,
					   0, nullptr, &size) != noErr ||
	    size == 0)
		return {};

	std::vector<AudioDeviceID> deviceIds(size / sizeof(AudioDeviceID));
	if (deviceIds.empty())
		return {};
	if (AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0,
				       nullptr, &size,
				       deviceIds.data()) != noErr)
		return {};

	std::vector<AudioInputDeviceInfo> devices;
	for (const AudioDeviceID deviceId : deviceIds) {
		const int channels = inputChannelCount(deviceId);
		if (channels <= 0)
			continue; // output-only device
		std::string name = deviceName(deviceId);
		if (name.empty())
			continue;
		devices.push_back({std::move(name), channels});
	}
	return devices;
}

} // namespace asrtu

#else

namespace asrtu
{
int macDefaultInputChannels()
{
	return 0;
}
std::vector<AudioInputDeviceInfo> macInputDevices()
{
	return {};
}
} // namespace asrtu

#endif
