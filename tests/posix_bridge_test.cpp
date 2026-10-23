#include "../integrations/sdrpp/asrtu_sink/src/posix_bridge.h"
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
	asrtu::storeShared(control, 0, std::uint32_t(0x504f4441U));
	asrtu::storeShared(control, 4, std::uint32_t(1));
	asrtu::storeShared(control, 8, std::int64_t(145800000));
	asrtu::storeShared(control, 16, std::int64_t(1250));
	asrtu::storeShared(control, 24, std::int64_t(now));
	asrtu::storeShared(control, 32, std::int32_t(1));
	asrtu::storeShared(control, 36, std::int32_t(2));
	munmap(control, 64);
	close(controlFd);

	asrtu::DopplerReader reader(controlPath);
	std::int64_t target = 0;
	std::int64_t correction = 0;
	if (!reader.read(target, correction) || target != 145800000 ||
	    correction != 1250)
		return 10;
	unlink(controlPath);
	return 0;
}
