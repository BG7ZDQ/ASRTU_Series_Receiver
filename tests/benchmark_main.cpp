#include "flowgraph.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <windows.h>
#include <psapi.h>

#include <chrono>
#include <algorithm>
#include <iostream>
#include <mutex>
#include <vector>

namespace {
double fileTimeSeconds(const FILETIME& time)
{
    ULARGE_INTEGER value{};
    value.LowPart = time.dwLowDateTime;
    value.HighPart = time.dwHighDateTime;
    return value.QuadPart / 10000000.0;
}
}

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    if (application.arguments().size() < 3 || application.arguments().size() > 6) {
        std::cerr << "Usage: ASRTU1_Benchmark <input.wav> <result.json> [if_hz] [--real-if] [--no-parallel] [--legacy-agc]\n";
        return 2;
    }

    const QString wavPath = QFileInfo(application.arguments().at(1)).absoluteFilePath();
    const QString outputPath = QFileInfo(application.arguments().at(2)).absoluteFilePath();
    std::mutex frameMutex;
    std::vector<QByteArray> frameHashes;
    std::vector<int> frameLengths;
    QString firstFrameLog;

    AsrtuFlowgraph::Options options;
    options.wav_path = wavPath.toStdString();
    for (const auto& argument : application.arguments().mid(3)) {
        if (argument == QStringLiteral("--no-parallel"))
            options.enable_parallel_decoder = false;
        else if (argument == QStringLiteral("--legacy-agc"))
            options.use_legacy_feedforward_agc = true;
        else if (argument == QStringLiteral("--real-if")) {
            options.real_if_12khz = true;
            options.input_frequency_hz = -12000.0;
        }
        else
            options.input_frequency_hz = argument.toDouble();
    }
    options.enable_gui = false;
    options.enable_network = false;
    options.payload_callback = [&](const std::vector<std::uint8_t>& payload) {
        const QByteArray bytes(reinterpret_cast<const char*>(payload.data()),
                               int(payload.size()));
        const QByteArray hash = QCryptographicHash::hash(
            bytes, QCryptographicHash::Sha256).toHex();
        std::lock_guard<std::mutex> lock(frameMutex);
        if (std::find(frameHashes.begin(), frameHashes.end(), hash) !=
            frameHashes.end())
            return;
        frameLengths.push_back(int(payload.size()));
        frameHashes.push_back(hash);
    };

    FILETIME createBefore{}, exitBefore{}, kernelBefore{}, userBefore{};
    GetProcessTimes(GetCurrentProcess(), &createBefore, &exitBefore,
                    &kernelBefore, &userBefore);
    const auto wallStart = std::chrono::steady_clock::now();

    AsrtuFlowgraph flowgraph([&](const std::string& line) {
        std::lock_guard<std::mutex> lock(frameMutex);
        if (firstFrameLog.isEmpty())
            firstFrameLog = QString::fromStdString(line);
    }, std::move(options));
    flowgraph.start();
    flowgraph.waitForCompletion();

    const double wallSeconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - wallStart).count();
    FILETIME createAfter{}, exitAfter{}, kernelAfter{}, userAfter{};
    GetProcessTimes(GetCurrentProcess(), &createAfter, &exitAfter,
                    &kernelAfter, &userAfter);
    PROCESS_MEMORY_COUNTERS_EX memory{};
    memory.cb = sizeof(memory);
    GetProcessMemoryInfo(GetCurrentProcess(),
                         reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),
                         sizeof(memory));

    QJsonArray hashes;
    QJsonArray lengths;
    {
        std::lock_guard<std::mutex> lock(frameMutex);
        for (const auto& hash : frameHashes)
            hashes.append(QString::fromLatin1(hash));
        for (const int length : frameLengths)
            lengths.append(length);
    }
    const double cpuSeconds =
        fileTimeSeconds(kernelAfter) + fileTimeSeconds(userAfter) -
        fileTimeSeconds(kernelBefore) - fileTimeSeconds(userBefore);
    QJsonObject result{
        { QStringLiteral("implementation"),
          options.use_legacy_feedforward_agc
              ? QStringLiteral("cqt-legacy-feedforward-agc")
              : QStringLiteral("cqt-causal-agc2") },
        { QStringLiteral("wav"), wavPath },
        { QStringLiteral("wall_seconds"), wallSeconds },
        { QStringLiteral("cpu_seconds"), cpuSeconds },
        { QStringLiteral("peak_working_set_bytes"),
          double(memory.PeakWorkingSetSize) },
        { QStringLiteral("frame_count"), hashes.size() },
        { QStringLiteral("primary_frame_count"),
          double(flowgraph.primaryFrameCount()) },
        { QStringLiteral("parallel_frame_count"),
          double(flowgraph.parallelFrameCount()) },
        { QStringLiteral("stereo_iq_content_mismatch"),
          flowgraph.stereoIqContentMismatch() },
        { QStringLiteral("final_svr_snr_db"), flowgraph.snr() },
        { QStringLiteral("first_frame_log"), firstFrameLog },
        { QStringLiteral("complete_223_count"),
          std::count(frameLengths.begin(), frameLengths.end(), 223) },
        { QStringLiteral("frame_lengths"), lengths },
        { QStringLiteral("frame_hashes"), hashes }
    };

    QFile output(outputPath);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        std::cerr << "Cannot write result JSON\n";
        return 3;
    }
    output.write(QJsonDocument(result).toJson(QJsonDocument::Indented));
    return 0;
}
