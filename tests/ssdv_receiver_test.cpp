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
                return updates > 0 && !last.image.isNull();
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
        !QFileInfo::exists(result.path) ||
        !QFileInfo(result.path).fileName().contains(QStringLiteral("ID43"))) {
        std::cerr << "SSDV reconstruction did not produce the expected image\n";
        return 5;
    }
    std::cout << "SSDV OK: " << result.path.toLocal8Bit().constData() << '\n';
    return 0;
}
