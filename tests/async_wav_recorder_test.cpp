#include "async_wav_recorder_sink.h"

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

namespace
{
std::uint32_t read32(const QByteArray &data, int offset)
{
	return std::uint32_t(std::uint8_t(data[offset])) |
	       (std::uint32_t(std::uint8_t(data[offset + 1])) << 8U) |
	       (std::uint32_t(std::uint8_t(data[offset + 2])) << 16U) |
	       (std::uint32_t(std::uint8_t(data[offset + 3])) << 24U);
}
} // namespace

int main()
{
    QTemporaryDir directory;
    if (!directory.isValid())
        return 1;
    const QString path = directory.filePath(QStringLiteral("录音测试.wav"));
    // 200 data bytes hold exactly 50 stereo PCM16 frames, so this small
    // limit exercises automatic RIFF segmentation without a multi-gigabyte
    // fixture.
    auto recorder = AsyncWavRecorderSink::make(path, 2, 48000, 200);
    std::vector<float> left(100, 0.5f);
    std::vector<float> right(100, -0.5f);
    gr_vector_const_void_star inputs{left.data(), right.data()};
    gr_vector_void_star outputs;
    if (recorder->work(100, inputs, outputs) != 100 || !recorder->stop())
        return 1;

    const QString secondPath = directory.filePath(
        QStringLiteral("录音测试_part02.wav"));
    for (const QString& segment : {path, secondPath}) {
        QFile file(segment);
        if (!file.open(QIODevice::ReadOnly))
            return 1;
        const QByteArray contents = file.readAll();
        if (contents.size() != 44 + 50 * 2 * 2 ||
            std::memcmp(contents.constData(), "RIFF", 4) != 0 ||
            std::memcmp(contents.constData() + 8, "WAVE", 4) != 0) {
            std::cerr << "invalid segmented asynchronous WAV output\n";
            return 1;
        }
    }

    // A WAV that is still being recorded must already carry a usable RIFF
    // size, so a crash or a forced kill loses at most the last refresh block
    // instead of the whole file. Feed exactly two 64 KiB blocks and inspect
    // the header before the recorder is stopped.
    const QString crashPath =
	directory.filePath(QStringLiteral("crash-safe.wav"));
    auto crashRecorder = AsyncWavRecorderSink::make(crashPath, 2, 48000);
    constexpr int blockFrames = 8192; // 8192 stereo PCM16 frames = 32 KiB
    std::vector<float> blockLeft(blockFrames, 0.25f);
    std::vector<float> blockRight(blockFrames, -0.25f);
    gr_vector_const_void_star blockInputs{blockLeft.data(), blockRight.data()};
    for (int block = 0; block < 4; ++block) {
	    if (crashRecorder->work(blockFrames, blockInputs, outputs) !=
		blockFrames)
		    return 1;
    }
    constexpr qint64 expectedDataBytes = 4 * blockFrames * 2 * 2;
    for (int attempt = 0; attempt < 300; ++attempt) {
	    const QFileInfo info(crashPath);
	    if (info.size() >= 44 + expectedDataBytes)
		    break;
	    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    QByteArray header;
    {
	    QFile file(crashPath);
	    if (!file.open(QIODevice::ReadOnly)) {
		    std::cerr
			<< "in-progress recording could not be opened for "
			   "reading\n";
		    return 1;
	    }
	    header = file.read(44);
    }
    const QFileInfo crashInfo(crashPath);
    if (header.size() != 44 ||
	std::memcmp(header.constData(), "RIFF", 4) != 0 ||
	std::memcmp(header.constData() + 8, "WAVE", 4) != 0 ||
	read32(header, 40) != std::uint32_t(expectedDataBytes) ||
	crashInfo.size() != 44 + expectedDataBytes) {
	    std::cerr << "recording header was not refreshed while recording\n";
	    return 1;
    }
    if (!crashRecorder->stop())
	    return 1;
    if (QFileInfo(crashPath).size() != 44 + expectedDataBytes)
	    return 1;
    return 0;
}
