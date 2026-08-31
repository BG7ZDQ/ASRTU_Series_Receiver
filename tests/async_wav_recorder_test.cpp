#include "async_wav_recorder_sink.h"

#include <QFile>
#include <QTemporaryDir>

#include <cstring>
#include <iostream>
#include <vector>

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
    return 0;
}
