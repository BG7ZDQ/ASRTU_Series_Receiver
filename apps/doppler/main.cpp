#include "satellite_tracker_dialog.h"
#include "translation.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QFont>
#include <QGuiApplication>
#include <QIcon>
#include <QTranslator>
#include <QTimer>

int main(int argc, char* argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif
    QApplication application(argc, argv);
    QTranslator translator;
    installSystemTranslation(application, translator);
    application.setWindowIcon(QIcon(QStringLiteral(":/icons/win98_doppler.png")));
    QFont font = application.font();
#ifdef Q_OS_WIN
    font.setFamily(QStringLiteral("Microsoft YaHei UI"));
#endif
    font.setPointSize(10);
    font.setStyleStrategy(static_cast<QFont::StyleStrategy>(
        QFont::PreferAntialias | QFont::PreferQuality));
    application.setFont(font);
    QCoreApplication::setOrganizationName(QStringLiteral("ASRTU"));
    QCoreApplication::setApplicationName(QStringLiteral("ASRTU_Doppler"));

    QCommandLineParser parser;
    parser.addHelpOption();
    const QCommandLineOption longitude(QStringLiteral("longitude"),
                                       QStringLiteral("Observer longitude"),
                                       QStringLiteral("degrees"), QStringLiteral("0"));
    const QCommandLineOption latitude(QStringLiteral("latitude"),
                                      QStringLiteral("Observer latitude"),
                                      QStringLiteral("degrees"), QStringLiteral("0"));
    const QCommandLineOption altitude(QStringLiteral("altitude"),
                                      QStringLiteral("Observer altitude"),
                                      QStringLiteral("metres"), QStringLiteral("0"));
    const QCommandLineOption satellite(QStringLiteral("satellite"),
                                       QStringLiteral("Preferred satellite"),
                                       QStringLiteral("name"), QStringLiteral("ASRTU-1"));
    const QCommandLineOption screenshot(QStringLiteral("screenshot"),
                                        QStringLiteral("Save a window screenshot"),
                                        QStringLiteral("path"));
    parser.addOptions({longitude, latitude, altitude, satellite, screenshot});
    parser.process(application);

    SatelliteTrackerDialog window(
        parser.value(longitude).toDouble(), parser.value(latitude).toDouble(),
        parser.value(altitude).toDouble(), parser.value(satellite));
    window.show();
    if (parser.isSet(screenshot)) {
        const QString path = parser.value(screenshot);
        QTimer::singleShot(1200, &window, [&application, &window, path] {
            window.grab().save(path);
            application.quit();
        });
    }
    return application.exec();
}
