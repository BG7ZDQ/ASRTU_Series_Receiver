#include "main_window.h"
#include "translation.h"

#include <QApplication>
#include <QCoreApplication>
#include <QFont>
#include <QGuiApplication>
#include <QIcon>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QLocalSocket>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTimer>
#include <QTranslator>
#include <cstdlib>
#include <exception>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {
bool lockOwnerIsLiveDecoder(QLockFile* lock)
{
    qint64 pid = 0;
    QString host;
    QString application;
    if (!lock->getLockInfo(&pid, &host, &application))
        return false; // Corrupt or incomplete lock file: safe to replace.
    if (pid <= 0)
        return false;

#ifdef Q_OS_WIN
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                 static_cast<DWORD>(pid));
    if (!process)
        return GetLastError() == ERROR_ACCESS_DENIED; // Assume live if protected.
    using QueryImageName = BOOL(WINAPI*)(HANDLE, DWORD, LPWSTR, PDWORD);
    const auto queryImage = reinterpret_cast<QueryImageName>(
        GetProcAddress(GetModuleHandleW(L"kernel32.dll"),
                       "QueryFullProcessImageNameW"));
    wchar_t executable[MAX_PATH] = {};
    DWORD length = static_cast<DWORD>(sizeof(executable) / sizeof(executable[0]));
    const bool queried = queryImage && queryImage(process, 0, executable, &length);
    CloseHandle(process);
    if (!queried)
        return application.startsWith(QStringLiteral("ASRTU1_Demod_CQt"));
    const QString name = QFileInfo(
        QString::fromWCharArray(executable, static_cast<int>(length))).fileName();
    return name.compare(QStringLiteral("ASRTU1_Demod_CQt.exe"),
                        Qt::CaseInsensitive) == 0;
#else
    const QFileInfo process(QStringLiteral("/proc/%1/comm").arg(pid));
    if (!process.exists())
        return false;
    QFile comm(process.filePath());
    if (!comm.open(QIODevice::ReadOnly))
        return true; // PID exists but is not inspectable; keep the lock.
    return QString::fromLatin1(comm.readAll()).trimmed().startsWith(
        QStringLiteral("ASRTU1_Demod_CQt"));
#endif
}
} // namespace

int main(int argc, char* argv[])
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
    QCoreApplication::setAttribute(Qt::AA_DisableWindowContextHelpButton);
#endif
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
    // Keep GNU Radio's FFTW wisdom with the application instead of scattering
    // it under %APPDATA%. Fall back only if the install folder is read-only.
    QString grCache = QDir(QCoreApplication::applicationDirPath())
                          .filePath(QStringLiteral("gnuradio-cache"));
    if (!QDir().mkpath(grCache)) {
        grCache = QDir(QStandardPaths::writableLocation(
                           QStandardPaths::AppLocalDataLocation))
                      .filePath(QStringLiteral("gnuradio-cache"));
        QDir().mkpath(grCache);
    }
    qputenv("GR_CACHE_PATH", QFile::encodeName(grCache));

    const QStringList arguments = QCoreApplication::arguments();
    const QString liveReceiverLockPath = QDir::temp().filePath(
        QStringLiteral("asrtu-live-demodulator.lock"));
    QLockFile liveReceiverLock(liveReceiverLockPath);
    // Live receivers commonly run for hours. Disable age-based expiry while
    // retaining QLockFile's dead-owner detection after an abnormal exit.
    liveReceiverLock.setStaleLockTime(0);
    if (!arguments.contains(QStringLiteral("--wav"))) {
        bool locked = liveReceiverLock.tryLock(100);
        if (!locked && !lockOwnerIsLiveDecoder(&liveReceiverLock)) {
            // A crashed decoder leaves a PID that Windows may reuse later.
            // Never let an unrelated process block all future live receivers.
            QFile::remove(liveReceiverLockPath);
            locked = liveReceiverLock.tryLock(100);
        }
        if (!locked)
            return 3;
    }

    try {
        MainWindow window;
        if (arguments.contains(QStringLiteral("--windowed")))
            window.show();
        else
            window.showMaximized();
        const int readyFileIndex = arguments.indexOf(
            QStringLiteral("--ready-file"));
        if (readyFileIndex >= 0 && readyFileIndex + 1 < arguments.size()) {
            QFile readyFile(arguments.at(readyFileIndex + 1));
            if (!readyFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                std::cerr << "Unable to write decoder readiness file\n";
                return EXIT_FAILURE;
            }
            readyFile.write(QByteArrayLiteral("ready"));
            if (!readyFile.flush()) {
                std::cerr << "Unable to flush decoder readiness file\n";
                return EXIT_FAILURE;
            }
            readyFile.close();
        }
        const int readyIndex = arguments.indexOf(
            QStringLiteral("--ready-server"));
        if (readyIndex >= 0 && readyIndex + 1 < arguments.size()) {
            QLocalSocket readySocket;
            readySocket.connectToServer(arguments.at(readyIndex + 1),
                                        QIODevice::WriteOnly);
            if (!readySocket.waitForConnected(3000)) {
                std::cerr << "Unable to signal decoder readiness\n";
                return EXIT_FAILURE;
            }
            readySocket.write(QByteArrayLiteral("ready"));
            if (!readySocket.waitForBytesWritten(1000)) {
                std::cerr << "Unable to write decoder readiness\n";
                return EXIT_FAILURE;
            }
            readySocket.disconnectFromServer();
        }
        if (argc >= 3 && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--screenshot")) {
            const QString screenshotPath = QString::fromLocal8Bit(argv[2]);
            QTimer::singleShot(1500, &window, [&app, &window, screenshotPath] {
                if (!window.grab().save(screenshotPath))
			std::cerr << "Unable to save UI screenshot" << '\n';
		app.quit();
            });
        }
        return app.exec();
    } catch (const std::exception &e) {
	    std::cerr << "Fatal error: " << e.what() << '\n';
	    QString message = QString::fromUtf8(e.what());
	    if (message.contains(
		    QStringLiteral("check topology failed on audio_"))) {
		    message =
			QCoreApplication::translate(
			    "ASRTU",
			    "无法按当前模式打开音频输入设备。\n\n"
			    "“立体声零中频 RAW 模式 I/Q "
			    "输入”必须由声卡提供两个录音声道；"
			    "若你的电台或麦克风只有一个声道，请在启动器中选择"
			    "“单声道实数域 12KHz 电台 IF "
			    "输入”。\n\n底层错误：%1")
			    .arg(message);
	    }
	    QMessageBox::critical(nullptr, QStringLiteral("ASRTU fatal error"),
				  message);
	    return EXIT_FAILURE;
    }
}
