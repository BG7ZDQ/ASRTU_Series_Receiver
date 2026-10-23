#include "../integrations/sdrpp/asrtu_sink/src/posix_bridge.h"
#include "../external/TinyDoppler/app/doppler_control_record.h"
#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <sys/wait.h>

int main()
{
	char path[] = "/tmp/asrtu-bridge-test-XXXXXX";
	const int fd = mkstemp(path);
	if (fd < 0)
		return 1;
	close(fd);
	asrtu::SharedBridge writer, contender;
	if (!writer.start(path) || contender.start(path))
		return 2;
	struct Sample {
		float i, q;
	};
	const std::array<Sample, 2> samples{{{0.25f, -0.5f}, {0.75f, -1.0f}}};
	writer.write(samples.data(), int(samples.size()));
	const auto child = fork();
	if (child < 0)
		return 3;
	if (child == 0) {
		const int input = open(path, O_RDONLY);
		const auto *base = static_cast<const std::uint8_t *>(mmap(
		    nullptr, asrtu::kBytes, PROT_READ, MAP_SHARED, input, 0));
		if (base == MAP_FAILED)
			_exit(4);
		const auto *received =
		    reinterpret_cast<const Sample *>(base + 64);
		const bool valid =
		    asrtu::loadShared<std::uint64_t>(base, 24) == 2 &&
		    asrtu::loadShared<std::uint32_t>(base, 40) == 1 &&
		    received[0].i == samples[0].i &&
		    received[0].q == samples[0].q &&
		    received[1].i == samples[1].i &&
		    received[1].q == samples[1].q;
		_exit(valid ? 0 : 5);
	}
	int status = 0;
	waitpid(child, &status, 0);
	if (!WIFEXITED(status) || WEXITSTATUS(status))
		return 6;
	writer.stop();
	if (!contender.start(path))
		return 7;
	contender.stop();
	unlink(path);

	char controlPath[] = "/tmp/asrtu-doppler-test-XXXXXX";
	const int controlFd = mkstemp(controlPath);
	if (controlFd < 0 || ftruncate(controlFd, 64))
		return 8;
	auto *control = static_cast<std::uint8_t *>(mmap(
	    nullptr, 64, PROT_READ | PROT_WRITE, MAP_SHARED, controlFd, 0));
	if (control == MAP_FAILED)
		return 9;
	std::memset(control, 0, 64);
	const auto now =
	    std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch())
		.count();
	tiny_doppler::publishControlRecord(control, 145800000, 1250, now, true);

	asrtu::DopplerReader reader(controlPath);
	std::int64_t target = 0;
	std::int64_t correction = 0;
	if (!reader.read(target, correction) || target != 145800000 ||
	    correction != 1250)
		return 10;
	// Reject a writer in progress, an invalid tracker, and stale data.
	asrtu::storeShared(control, 36, std::uint32_t(3));
	if (reader.read(target, correction))
		return 11;
	tiny_doppler::publishControlRecord(control, 145800000, 1250, now,
					   false);
	if (reader.read(target, correction))
		return 12;
	tiny_doppler::publishControlRecord(control, 145800000, 1250, now - 4000,
					   true);
	if (reader.read(target, correction))
		return 13;
	// Sequence wrap is defined, and a stale reader can reconnect.
	asrtu::storeShared(control, 36, UINT32_MAX - 1);
	tiny_doppler::publishControlRecord(control, 145800000, 1250, now, true);
	if (!reader.read(target, correction) ||
	    asrtu::loadShared<std::uint32_t>(control, 36) != 0)
		return 14;

	// Exercise the production writer against the actual SDR++ reader in
	// another process. Every accepted record must be a coherent pair.
	tiny_doppler::publishControlRecord(control, 145800000, -145800000, now,
					   true);
	const auto publisher = fork();
	if (publisher < 0)
		return 15;
	if (publisher == 0) {
		for (std::int64_t i = 1; i <= 200000; ++i)
			tiny_doppler::publishControlRecord(
			    control, 145800000 + i, -(145800000 + i), now,
			    true);
		_exit(0);
	}
	bool coherent = true;
	for (int i = 0; i < 200000; ++i) {
		if (reader.read(target, correction) && target != -correction)
			coherent = false;
	}
	waitpid(publisher, &status, 0);
	if (!coherent || !WIFEXITED(status) || WEXITSTATUS(status))
		return 16;
	munmap(control, 64);
	close(controlFd);
	reader.close();
	unlink(controlPath);
	return 0;
}
