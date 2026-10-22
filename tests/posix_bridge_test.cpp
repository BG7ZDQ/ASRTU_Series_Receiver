#include "../integrations/sdrpp/asrtu_sink/src/posix_bridge.h"
#include <array>
#include <cstdio>
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
		const bool valid =
		    asrtu::loadShared<std::uint64_t>(base, 24) == 2 &&
		    asrtu::loadShared<std::uint32_t>(base, 40) == 1 &&
		    std::memcmp(base + 64, samples.data(), sizeof(samples)) ==
			0;
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
	return 0;
}
