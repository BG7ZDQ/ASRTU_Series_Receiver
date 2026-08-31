#include "satnogs_uploader_window.h"
#include "translation.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QIcon>
#include <QDir>
#include <QLockFile>
#include <QMessageBox>
#include <QTimer>
#include <QTranslator>

#include <utility>

int main(int argc, char* argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("ASRTU"));
    QCoreApplication::setApplicationName(QStringLiteral("ASRTU_SatnogsUploader"));
    QTranslator translator;
    installSystemTranslation(application, translator);
    application.setWindowIcon(QIcon(QStringLiteral(":/icons/win98_proxy.png")));

    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption zmq(QStringLiteral("zmq"),
                                 QStringLiteral("ZeroMQ publisher address"),
                                 QStringLiteral("address"),
                                 QStringLiteral("tcp://127.0.0.1:5555"));
    const QCommandLineOption api(QStringLiteral("api"),
                                 QStringLiteral("SatNOGS telemetry API address"),
                                 QStringLiteral("url"),
                                 QStringLiteral("https://db.satnogs.org/api/telemetry/"));
    const QCommandLineOption satellite(QStringLiteral("satellite"),
                                       QStringLiteral("Satellite display name"),
                                       QStringLiteral("name"));
    const QCommandLineOption norad(QStringLiteral("norad-id"),
                                   QStringLiteral("Satellite NORAD catalog ID"),
                                   QStringLiteral("id"));
    const QCommandLineOption source(QStringLiteral("source"),
                                    QStringLiteral("Ground-station callsign"),
                                    QStringLiteral("callsign"));
    const QCommandLineOption longitude(QStringLiteral("longitude"),
                                       QStringLiteral("Ground-station longitude"),
                                       QStringLiteral("degrees"));
    const QCommandLineOption latitude(QStringLiteral("latitude"),
                                      QStringLiteral("Ground-station latitude"),
                                      QStringLiteral("degrees"));
    const QCommandLineOption altitude(QStringLiteral("altitude"),
                                      QStringLiteral("Ground-station altitude"),
                                      QStringLiteral("metres"), QStringLiteral("0"));
    const QCommandLineOption screenshot(QStringLiteral("screenshot"),
                                        QStringLiteral("Save a preview screenshot"),
                                        QStringLiteral("path"));
    parser.addOptions({zmq, api, satellite, norad, source, longitude,
                       latitude, altitude, screenshot});
    parser.process(application);

    asrtu::SatnogsUploaderConfig config;
    config.zmqAddress = parser.value(zmq);
    config.apiAddress = parser.value(api);
    config.satellite = parser.value(satellite);
    config.noradId = parser.value(norad).toInt();
    config.source = parser.value(source);
    config.longitude = parser.value(longitude).toDouble();
    config.latitude = parser.value(latitude).toDouble();
    config.altitude = parser.value(altitude).toDouble();

    QString error;
    if (!asrtu::validateSatnogsUploaderConfig(config, &error)) {
        QMessageBox::critical(nullptr,
                              QCoreApplication::translate(
                                  "ASRTU", "SatNOGS 上传配置无效"),
                              error);
        return 2;
    }

    QLockFile instanceLock(QDir::temp().filePath(
        QStringLiteral("asrtu-satnogs-uploader.lock")));
    // These processes are intentionally long-lived. Never expire a healthy
    // lock based only on its file age; QLockFile can still remove it when the
    // recorded owner process no longer exists.
    instanceLock.setStaleLockTime(0);
    if (!instanceLock.tryLock(100)) {
        // A second subscriber would upload every decoded frame again. Exit
        // promptly so the launcher can report that an uploader is already
        // active instead of treating a modal error window as a live worker.
        return 3;
    }

    asrtu::SatnogsUploaderWindow window(std::move(config));
    if (!window.start(&error)) {
        QMessageBox::critical(nullptr,
                              QCoreApplication::translate(
                                  "ASRTU", "SatNOGS 上传程序启动失败"),
                              error);
        return 1;
    }
    window.show();

    if (parser.isSet(screenshot)) {
        QByteArray preview(223, Qt::Uninitialized);
        for (int i = 0; i < preview.size(); ++i)
            preview[i] = static_cast<char>((i * 37 + 11) & 0xff);
        window.showPreviewFrame(preview);
        const QString path = parser.value(screenshot);
        QTimer::singleShot(900, &window, [&application, &window, path] {
            window.grab().save(path);
            application.quit();
        });
    }
    return application.exec();
}
