#include "satnogs_uploader_window.h"

#include <QByteArray>
#include <QNetworkReply>
#include <QString>

#include <iostream>

namespace
{
bool expect(bool condition, const char* message)
{
    if (condition)
        return true;
    std::cerr << message << '\n';
    return false;
}
} // namespace

int main()
{
    asrtu::SatnogsUploaderConfig config;
    config.satellite = QStringLiteral("JAMX");
    config.source = QStringLiteral("CI-TEST");
    config.noradId = 98248;
    config.longitude = 0.0;
    config.latitude = 0.0;
    config.altitude = 0.0;

    QString error;
    if (!expect(asrtu::validateSatnogsUploaderConfig(config, &error),
                "valid SatNOGS uploader configuration was rejected"))
        return 1;
    config.latitude = 91.0;
    if (!expect(!asrtu::validateSatnogsUploaderConfig(config, &error),
                "out-of-range latitude was accepted"))
        return 1;
    config.latitude = 0.0;
    config.zmqAddress = QStringLiteral("ipc://unsupported");
    if (!expect(!asrtu::validateSatnogsUploaderConfig(config, &error),
                "unsupported ZMQ transport was accepted"))
        return 1;

    const QByteArray frame = QByteArray::fromHex(
        "00A55AFF0102030405060708090A0B0C0D0E0F10");
    const QString dump = asrtu::formatTelemetryFrame(frame);
    if (!expect(dump.contains(QStringLiteral("长度：20 字节")),
                "frame summary is missing the byte count") ||
        !expect(dump.contains(QStringLiteral(
                    "0000: 00 A5 5A FF 01 02 03 04 05 06 07 08 09 0A 0B 0C")),
                "frame dump is missing the first hexadecimal row") ||
        !expect(dump.contains(QStringLiteral("0010: 0D 0E 0F 10")),
                "frame dump is missing the second hexadecimal row")) {
        return 1;
    }
    if (!expect(asrtu::isRetriableSatnogsUpload(
                    int(QNetworkReply::TimeoutError), 0),
                "network timeout was not retriable") ||
        !expect(asrtu::isRetriableSatnogsUpload(
                    int(QNetworkReply::ContentAccessDenied), 429),
                "HTTP 429 was not retriable") ||
        !expect(!asrtu::isRetriableSatnogsUpload(
                    int(QNetworkReply::ContentNotFoundError), 404),
                "HTTP 404 was incorrectly retriable") ||
        !expect(asrtu::satnogsRetryDelayMs(1) == 1000 &&
                    asrtu::satnogsRetryDelayMs(5) == 16000 &&
                    asrtu::satnogsRetryDelayMs(2, 60) == 60000,
                "retry backoff calculation is incorrect")) {
        return 1;
    }
    return 0;
}
