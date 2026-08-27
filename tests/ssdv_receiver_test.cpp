#include "ssdv_receiver.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <iostream>

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    if (argc != 3) {
        std::cerr << "usage: ASRTU1_SsdvTest <223-byte-frame-file> <output-dir>\n";
        return 2;
    }

    QFile input(QString::fromLocal8Bit(argv[1]));
    if (!input.open(QIODevice::ReadOnly))
        return 3;
    const QByteArray data = input.readAll();
    if (data.size() % 223 != 0)
        return 4;
    QDir().mkpath(QString::fromLocal8Bit(argv[2]));

    SsdvImageUpdate last;
    int updates = 0;
    SsdvReceiver receiver(QString::fromLocal8Bit(argv[2]),
        [&](const SsdvImageUpdate& update) {
            last = update;
            ++updates;
        });
    for (int offset = 0; offset < data.size(); offset += 223)
        receiver.ingestFrame(data.mid(offset, 223));

    if (updates == 0 || last.image.isNull() || last.image_id != 0x2b ||
        last.satellite != QStringLiteral("BY-04") || last.spacecraft_header != 0x2052 ||
        last.width != 320 || last.height != 240 || last.received_packets != 25 ||
        last.missing_packets != 1 || !last.complete ||
        !QFileInfo::exists(last.path) ||
        !QFileInfo(last.path).fileName().contains(QStringLiteral("ID43"))) {
        std::cerr << "SSDV reconstruction did not produce the expected image\n";
        return 5;
    }
    std::cout << "SSDV OK: " << last.path.toLocal8Bit().constData() << '\n';
    return 0;
}
