#include "ssdv_receiver.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>

namespace {
constexpr std::uint32_t kJamxCrcInitialValue = 0x6AAAC1C5U;

std::uint32_t ssdvCrc32(const char* data, std::size_t length,
                        std::uint32_t initialValue)
{
    std::uint32_t crc = initialValue;
    for (std::size_t offset = 0; offset < length; ++offset) {
        std::uint32_t value =
            (crc ^ std::uint8_t(data[offset])) & 0xFFU;
        for (int bit = 0; bit < 8; ++bit)
            value = (value & 1U) != 0U
                        ? (value >> 1U) ^ 0xEDB88320U
                        : value >> 1U;
        crc = (crc >> 8U) ^ value;
    }
    return crc ^ 0xFFFFFFFFU;
}

QByteArray convertToJamxCrc(QByteArray frames)
{
    constexpr int frameSize = 223;
    constexpr int packetOffset = 5;
    constexpr int crcDataSize = 214;
    for (int offset = 0; offset < frames.size(); offset += frameSize) {
        const auto crc = ssdvCrc32(frames.constData() + offset + packetOffset,
                                   crcDataSize, kJamxCrcInitialValue);
        const int crcOffset = offset + packetOffset + crcDataSize;
        frames[crcOffset] = char((crc >> 24U) & 0xFFU);
        frames[crcOffset + 1] = char((crc >> 16U) & 0xFFU);
        frames[crcOffset + 2] = char((crc >> 8U) & 0xFFU);
        frames[crcOffset + 3] = char(crc & 0xFFU);
    }
    return frames;
}

}

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    const QString inputPath = argc >= 2
                                  ? QString::fromLocal8Bit(argv[1])
                                  : QStringLiteral(ASRTU_TEST_FIXTURE);
    QTemporaryDir temporaryOutput;
    const QString outputPath = argc >= 3
                                   ? QString::fromLocal8Bit(argv[2])
                                   : temporaryOutput.path();
    if (outputPath.isEmpty())
        return 2;

    QFile input(inputPath);
    if (!input.open(QIODevice::ReadOnly))
        return 3;
    QByteArray data = input.readAll();
    if (inputPath.endsWith(QStringLiteral(".b64")))
        data = QByteArray::fromBase64(data);
    if (data.size() % 223 != 0)
        return 4;
    QDir().mkpath(outputPath);

    SsdvImageUpdate last;
    int updates = 0;
    std::mutex resultMutex;
    std::condition_variable resultReady;
    SsdvReceiver receiver(outputPath,
        [&](const SsdvImageUpdate& update) {
            {
                std::lock_guard<std::mutex> lock(resultMutex);
                last = update;
                ++updates;
            }
            resultReady.notify_all();
        });
    for (int offset = 0; offset < data.size(); offset += 223)
        receiver.ingestFrame(data.mid(offset, 223));

    SsdvImageUpdate result;
    {
        std::unique_lock<std::mutex> lock(resultMutex);
        if (!resultReady.wait_for(lock, std::chrono::seconds(3), [&] {
                return updates > 0 && !last.image.isNull() &&
                       last.received_packets >= 20;
            })) {
            std::cerr << "SSDV reconstruction timed out\n";
            return 5;
        }
        result = last;
    }

    if (result.image.isNull() || result.image_id != 0x2b ||
        result.satellite != QStringLiteral("ASRTU-1") ||
        result.spacecraft_header != 0x0322 || result.width != 320 ||
        result.height != 240 || result.received_packets < 20 ||
        !result.complete ||
        !QFileInfo::exists(result.path) ||
        !QFileInfo(result.path).fileName().contains(QStringLiteral("ID43"))) {
        std::cerr << "SSDV reconstruction did not produce the expected image\n";
        return 5;
    }

    // The fixture begins at packet 34 and ends at packet 59. Replaying it
    // exercises a non-zero counter regression (59 -> 34): this must create a
    // fresh image even though its image ID and packet contents are unchanged.
    for (int offset = 0; offset < data.size(); offset += 223)
        receiver.ingestFrame(data.mid(offset, 223));
    {
        std::unique_lock<std::mutex> lock(resultMutex);
        if (!resultReady.wait_for(lock, std::chrono::seconds(3), [&] {
                return !last.image.isNull() &&
                       last.received_packets == result.received_packets &&
                       last.path != result.path;
            })) {
            std::cerr << "SSDV packet-counter regression did not start a new image\n";
            return 6;
        }
    }

    for (int repeat = 0; repeat < 64; ++repeat) {
        for (int offset = 0; offset < data.size(); offset += 223)
            receiver.ingestFrame(data.mid(offset, 223));
    }
    receiver.clear();
    for (int offset = 0; offset < data.size(); offset += 223)
        receiver.ingestFrame(data.mid(offset, 223));
    {
        std::unique_lock<std::mutex> lock(resultMutex);
        if (!resultReady.wait_for(lock, std::chrono::seconds(3), [&] {
                return last.generation == 1 && !last.image.isNull() &&
                       last.received_packets == result.received_packets &&
                       last.path != result.path;
            })) {
            std::cerr << "SSDV clear did not discard the previous session\n";
            return 7;
        }
    }

    QByteArray second = data;
    for (int offset = 0; offset < second.size(); offset += 223) {
        second[offset] = char(0x20);
        second[offset + 1] = char(0x52);
    }
    for (int offset = 0; offset < second.size(); offset += 223)
        receiver.ingestFrame(second.mid(offset, 223));
    {
        std::unique_lock<std::mutex> lock(resultMutex);
        if (!resultReady.wait_for(lock, std::chrono::seconds(3), [&] {
                return last.spacecraft_header == 0x2052 &&
                       last.satellite == QStringLiteral("BY-04");
            })) {
            std::cerr << "SSDV session identity did not change\n";
            return 8;
        }
    }

    receiver.clear();
    const QByteArray jamx = convertToJamxCrc(data);
    for (int offset = 0; offset < jamx.size(); offset += 223)
        receiver.ingestFrame(jamx.mid(offset, 223));
    {
        std::unique_lock<std::mutex> lock(resultMutex);
        if (!resultReady.wait_for(lock, std::chrono::seconds(3), [&] {
                return last.generation == 2 &&
                       last.satellite == QStringLiteral("JAMX") &&
                       last.spacecraft_header == 0x0322 &&
                       !last.image.isNull() && last.complete;
            })) {
            std::cerr << "JAMX CRC image reconstruction timed out\n";
            return 9;
        }
        if (last.crc_failed_packets != 0 ||
            !last.crc_failed_packet_ids.empty()) {
            std::cerr << "Valid JAMX CRC packets were marked as failed\n";
            return 10;
        }
    }
    std::cout << "SSDV OK: " << result.path.toLocal8Bit().constData() << '\n';
    return 0;
}
