#include "main_window.h"
#include "translation.h"

#include <QApplication>
#include <QCoreApplication>
#include <QFont>
#include <QGuiApplication>
#include <QIcon>
#include <QMessageBox>
#include <QTimer>
#include <QTranslator>
#include <cstdlib>
#include <exception>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char* argv[])
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif
    QApplication app(argc, argv);
    QTranslator translator;
    installSystemTranslation(app, translator);
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/win98_dsp.png")));
    QFont uiFont = app.font();
#ifdef _WIN32
    uiFont.setFamily(QStringLiteral("Microsoft YaHei UI"));
    uiFont.setPointSize(9);
#endif
    uiFont.setStyleStrategy(static_cast<QFont::StyleStrategy>(
        QFont::PreferAntialias | QFont::PreferQuality));
    uiFont.setHintingPreference(QFont::PreferFullHinting);
    app.setFont(uiFont);
    QCoreApplication::setOrganizationName(QStringLiteral("ASRTU"));
    QCoreApplication::setApplicationName(QStringLiteral("ASRTU1_Demod_CQt"));

    try {
        MainWindow window;
        const QStringList arguments = QCoreApplication::arguments();
        if (arguments.contains(QStringLiteral("--windowed")))
            window.show();
        else
            window.showMaximized();
        if (argc >= 3 && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--screenshot")) {
            const QString screenshotPath = QString::fromLocal8Bit(argv[2]);
            QTimer::singleShot(1500, &window, [&app, &window, screenshotPath] {
                if (!window.grab().save(screenshotPath))
                    std::cerr << "Unable to save UI screenshot" << std::endl;
                app.quit();
            });
        }
        return app.exec();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        QMessageBox::critical(nullptr, QStringLiteral("ASRTU fatal error"),
                              QString::fromUtf8(e.what()));
        return EXIT_FAILURE;
    }
}
