#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDataStream>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollArea>
#include <QSettings>
#include <QScreen>
#include <QSizePolicy>
#include <QStyle>
#include <QTextStream>
#include <QTimer>
#include <QTranslator>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include "satellite_tracker_dialog.h"
#include "translation.h"

#include <algorithm>
#include <cmath>

#ifdef Q_OS_WIN
#include <windows.h>
#include <mmsystem.h>
#endif

namespace {
QString decoderDirectory()
{
    return QCoreApplication::applicationDirPath();
}

QString proxyDirectory()
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/../proxy");
}

QString sdrSharpDirectory()
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/../sdrsharp");
}

QString configPath()
{
    return proxyDirectory() + QStringLiteral("/config.cfg");
}

struct SatelliteProfile {
    QString name;
    QString webSocket;
    int noradId;
};

QList<SatelliteProfile> satelliteProfiles()
{
    return {
        {QStringLiteral("ASRTU-1"), QStringLiteral("ws://1.92.100.130"), 61781},
        {QStringLiteral("BY-04uv"), QStringLiteral("ws://119.45.229.166"), 98247}
    };
}

int noradForSatellite(const QString& name)
{
    for (const auto& profile : satelliteProfiles()) {
        if (profile.name.compare(name, Qt::CaseInsensitive) == 0)
            return profile.noradId;
    }
    return 0;
}

QString webSocketForSatellite(const QString& name)
{
    for (const auto& profile : satelliteProfiles()) {
        if (profile.name.compare(name, Qt::CaseInsensitive) == 0)
            return profile.webSocket;
    }
    return satelliteProfiles().first().webSocket;
}

QString recordsDirectory();

QString newestRecordingPath()
{
    QDir root(recordsDirectory());
    QFileInfo newest;
    const auto sessions = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot,
                                             QDir::Time);
    for (const QFileInfo& session : sessions) {
        const QFileInfo candidate(QDir(session.absoluteFilePath()).filePath(
            QStringLiteral("recording.wav")));
        // A finalized PCM WAV contains at least its header and some samples.
        if (candidate.isFile() && candidate.size() > 44 &&
            (!newest.exists() || candidate.lastModified() > newest.lastModified()))
            newest = candidate;
    }
    return newest.absoluteFilePath();
}

QString recordsDirectory()
{
    return QDir::cleanPath(
        QDir(decoderDirectory()).absoluteFilePath(QStringLiteral("../ASRTU1_Records")));
}

QString escapedConfigString(QString value)
{
    value.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    value.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return value;
}

QString createSessionDirectory(QString* error)
{
    const QString directory = QDir(recordsDirectory()).filePath(
        QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz")));
    if (!QDir().mkpath(directory)) {
        *error = QCoreApplication::translate("ASRTU", "无法创建录音和日志目录：\n%1").arg(directory);
        return {};
    }
    return QDir(directory).absolutePath();
}

QString compactSessionDirectory(const QString& directory)
{
    return QStringLiteral("/%1").arg(QFileInfo(directory).fileName());
}

int wavChannelCount(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return 0;
    QDataStream stream(&file);
    stream.setByteOrder(QDataStream::LittleEndian);
    char riff[4] = {};
    char wave[4] = {};
    quint32 riffSize = 0;
    if (stream.readRawData(riff, 4) != 4)
        return 0;
    stream >> riffSize;
    if (stream.readRawData(wave, 4) != 4 || QByteArray(riff, 4) != "RIFF" ||
        QByteArray(wave, 4) != "WAVE")
        return 0;
    Q_UNUSED(riffSize)
    while (!stream.atEnd()) {
        char chunkId[4] = {};
        quint32 chunkSize = 0;
        if (stream.readRawData(chunkId, 4) != 4)
            break;
        stream >> chunkSize;
        if (QByteArray(chunkId, 4) == "fmt " && chunkSize >= 4) {
            quint16 format = 0;
            quint16 channels = 0;
            stream >> format >> channels;
            Q_UNUSED(format)
            return int(channels);
        }
        const qint64 skip = qint64(chunkSize) + (chunkSize & 1U);
        if (!file.seek(file.pos() + skip))
            break;
    }
    return 0;
}

bool startProgram(const QString& executable, const QStringList& arguments,
                  const QString& workingDirectory, const QString& outputLog,
                  bool visibleConsole, qint64* processId, QString* error)
{
    if (!QFileInfo::exists(executable)) {
        *error = QCoreApplication::translate("ASRTU", "找不到程序：\n%1").arg(executable);
        return false;
    }
    QProcess process;
    process.setProgram(executable);
    process.setArguments(arguments);
    process.setWorkingDirectory(workingDirectory);
    if (!outputLog.isEmpty()) {
        process.setStandardOutputFile(outputLog, QIODevice::Append);
        process.setStandardErrorFile(outputLog, QIODevice::Append);
    }
    if (!outputLog.isEmpty() || visibleConsole) {
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("TERM"), QStringLiteral("xterm"));
        environment.insert(QStringLiteral("TERMINFO"),
                           QDir(workingDirectory).filePath(QStringLiteral("terminfo")));
        process.setProcessEnvironment(environment);
#ifdef Q_OS_WIN
        process.setCreateProcessArgumentsModifier([visibleConsole](QProcess::CreateProcessArguments* args) {
            if (visibleConsole) {
                args->flags &= ~CREATE_NO_WINDOW;
                args->flags |= CREATE_NEW_CONSOLE;
            } else {
                args->flags &= ~CREATE_NEW_CONSOLE;
                args->flags |= CREATE_NO_WINDOW;
            }
        });
#endif
    }
    if (!process.startDetached(processId)) {
        *error = QCoreApplication::translate("ASRTU", "无法启动：\n%1").arg(executable);
        return false;
    }
    return true;
}

bool startSdrSharp(qint64* processId, QString* error)
{
    const QString executable = sdrSharpDirectory() + QStringLiteral("/SDRSharp.exe");
    return startProgram(executable, {}, sdrSharpDirectory(), {}, false,
                        processId, error);
}

QString logTail(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    if (file.size() > 4096)
        file.seek(file.size() - 4096);
    return QString::fromLocal8Bit(file.readAll()).trimmed();
}

void appendProxyLaunchLog(const QString& path, const QString& message)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        return;
    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    stream << QDateTime::currentDateTime().toString(
                  QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz "))
           << message << '\n';
}

bool processSurvivedStartup(qint64 processId, int timeoutMs, quint32* exitCode)
{
#ifdef Q_OS_WIN
    const HANDLE process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION,
                                       FALSE, DWORD(processId));
    if (!process)
        return true;
    const DWORD waitResult = WaitForSingleObject(process, DWORD(timeoutMs));
    DWORD nativeExitCode = STILL_ACTIVE;
    GetExitCodeProcess(process, &nativeExitCode);
    CloseHandle(process);
    if (exitCode)
        *exitCode = quint32(nativeExitCode);
    return waitResult == WAIT_TIMEOUT;
#else
    Q_UNUSED(processId)
    Q_UNUSED(timeoutMs)
    if (exitCode)
        *exitCode = 0;
    return true;
#endif
}

bool startProxy(const QString& launchLog, qint64* processId, QString* error)
{
    const QString proxyWrapper = proxyDirectory() + QStringLiteral("/ASRTU_Proxy.exe");
    const QString proxy = QFileInfo::exists(proxyWrapper)
                              ? proxyWrapper
                              : proxyDirectory() + QStringLiteral("/proxy_mmt_gui.exe");
    if (!QFileInfo::exists(configPath())) {
        *error = QCoreApplication::translate("ASRTU", "尚未配置上传信息，请先保存设置。");
        return false;
    }
    if (!launchLog.isEmpty()) {
        appendProxyLaunchLog(launchLog,
                             QStringLiteral("Starting interactive proxy console: %1")
                                 .arg(proxy));
    }
    qint64 proxyPid = 0;
    if (!startProgram(proxy, {}, proxyDirectory(), {}, true, &proxyPid, error))
        return false;
    quint32 exitCode = 0;
    if (!processSurvivedStartup(proxyPid, 1200, &exitCode)) {
        *error = QCoreApplication::translate("ASRTU", "上传代理启动后立即退出（代码 %1）。")
                     .arg(exitCode);
        if (!launchLog.isEmpty()) {
            const QString output = logTail(launchLog);
            *error += QCoreApplication::translate("ASRTU", "\n日志：%1").arg(launchLog);
            if (!output.isEmpty())
                *error += QCoreApplication::translate("ASRTU", "\n\n最后输出：\n%1").arg(output);
        }
        return false;
    }
    if (!launchLog.isEmpty()) {
        appendProxyLaunchLog(launchLog,
                             QStringLiteral("Proxy startup check passed; PID=%1")
                                 .arg(proxyPid));
    }
    *processId = proxyPid;
    return true;
}

bool startSuite(bool enableProxy, int inputMode, const QString& wavPath,
                int audioDeviceId, bool recordingEnabled,
                bool satnogsEnabled, const QString& satnogsSatellite,
                const QString& satnogsSource, double satnogsLongitude,
                double satnogsLatitude,
                QString* sessionDirectory,
                qint64* proxyProcessId, QString* error)
{
    const QString decoder = decoderDirectory() + QStringLiteral("/ASRTU1_Demod_CQt.exe");
    *sessionDirectory = createSessionDirectory(error);
    if (sessionDirectory->isEmpty())
        return false;
    QString playbackPath = wavPath;
    if (!playbackPath.isEmpty()) {
        if (QFileInfo(playbackPath).suffix().compare(
                QStringLiteral("wav"), Qt::CaseInsensitive) != 0) {
            const QString converter = decoderDirectory() +
                QStringLiteral("/sndfile-convert.exe");
            if (!QFileInfo::exists(converter)) {
                *error = QCoreApplication::translate("ASRTU", "找不到录音格式转换器。 ");
                return false;
            }
            const QString converted = QDir(*sessionDirectory).filePath(
                QStringLiteral("playback_input.wav"));
            QProcess conversion;
            conversion.setProgram(converter);
            conversion.setArguments({QStringLiteral("-pcm16"), playbackPath, converted});
            conversion.setWorkingDirectory(decoderDirectory());
            conversion.start();
            if (!conversion.waitForStarted(3000) ||
                !conversion.waitForFinished(60000) ||
                conversion.exitStatus() != QProcess::NormalExit ||
                conversion.exitCode() != 0) {
                *error = QCoreApplication::translate("ASRTU", "无法转换录音文件：/%1")
                             .arg(QFileInfo(playbackPath).fileName());
                return false;
            }
            playbackPath = converted;
        }
        const int channels = wavChannelCount(playbackPath);
        if (channels != 1 && channels != 2) {
            *error = QCoreApplication::translate("ASRTU", "仅支持单声道或双声道录音文件。");
            return false;
        }
        inputMode = channels == 1 ? 1 : 0;
    }
    qint64 proxyPid = 0;
    if (enableProxy) {
        const QString proxyLog = QDir(*sessionDirectory).filePath(
            QStringLiteral("proxy.log"));
        if (!startProxy(proxyLog, &proxyPid, error))
            return false;
    }
    qint64 decoderPid = 0;
    QStringList decoderArguments{
        QStringLiteral("--session-dir"), *sessionDirectory
    };
    if (!playbackPath.isEmpty())
        decoderArguments << QStringLiteral("--wav") << playbackPath;
    else if (!recordingEnabled)
        decoderArguments << QStringLiteral("--no-record");
    if (inputMode == 1)
        decoderArguments << QStringLiteral("--real-if-12k");
    else if (inputMode == 2)
        decoderArguments << QStringLiteral("--sdrsharp-iq-bridge");
    if (playbackPath.isEmpty() && inputMode != 2)
        decoderArguments << QStringLiteral("--audio-device")
                         << QString::number(audioDeviceId);
    if (satnogsEnabled) {
        const int noradId = noradForSatellite(satnogsSatellite);
        if (noradId <= 0) {
            *error = QCoreApplication::translate("ASRTU", "所选卫星没有可用的 SatNOGS NORAD ID。");
            return false;
        }
        decoderArguments << QStringLiteral("--satnogs-norad") << QString::number(noradId)
                         << QStringLiteral("--satnogs-source") << satnogsSource
                         << QStringLiteral("--satnogs-longitude")
                         << QString::number(satnogsLongitude, 'f', 6)
                         << QStringLiteral("--satnogs-latitude")
                         << QString::number(satnogsLatitude, 'f', 6);
    }
    if (!startProgram(decoder, decoderArguments,
                      decoderDirectory(), {}, false, &decoderPid, error))
        return false;
    *proxyProcessId = proxyPid;
    return true;
}

class LauncherWindow final : public QWidget
{
public:
    LauncherWindow()
    {
        // Keep the launcher available while decoder/proxy windows are created.
        setWindowIcon(QIcon(QStringLiteral(":/launcher/astro_series_launcher.png")));
        setWindowTitle(QCoreApplication::translate("ASRTU", "阿斯图系列卫星启动器"));
        // Keep the launcher compact on low-resolution and high-DPI displays.
        // The scroll area below keeps all controls reachable at the minimum size.
        setMinimumSize(260, 300);
        setStyleSheet(QStringLiteral(
            "QWidget#launcherRoot { background:#f3f6f9; color:#17202a; }"
            "QLabel#subtitle { color:#667085; }"
            "QFrame#card { background:#ffffff; border:1px solid #dce3ea; "
            "border-radius:8px; }"
            "QLabel#sectionTitle { color:#344054; font-weight:600; }"
            "QLineEdit,QDoubleSpinBox,QComboBox { min-height:30px; padding:0 9px; "
            "background:#ffffff; border:1px solid #cbd5e1; border-radius:5px; "
            "selection-background-color:#2b7de9; }"
            "QLineEdit:focus,QDoubleSpinBox:focus,QComboBox:focus { "
            "border:1px solid #2b7de9; }"
            "QComboBox::drop-down { border:0; width:25px; }"
            "QComboBox::down-arrow { image:url(:/launcher/combo_arrow.png); "
            "width:16px; height:16px; }"
            "QPushButton { min-height:34px; padding:0 16px; color:#344054; "
            "background:#ffffff; border:1px solid #cbd5e1; border-radius:6px; }"
            "QPushButton:hover { background:#f7f9fc; border-color:#98a6b5; }"
            "QPushButton:pressed { background:#edf1f5; }"
            "QPushButton#tool { min-height:38px; background:#f8fafc; "
            "border-color:#d8e0e8; font-weight:500; }"
            "QPushButton#tool:hover { background:#eef5ff; border-color:#80aee8; "
            "color:#145ca8; }"
            "QPushButton#primary { min-height:38px; color:#ffffff; "
            "background:#16834f; border:1px solid #16834f; font-weight:600; }"
            "QPushButton#primary:hover { background:#116f42; border-color:#116f42; }"
            "QPushButton#primary:pressed { background:#0d5d37; }"
            "QPushButton#secondaryAction { min-height:38px; color:#145ca8; "
            "background:#edf5ff; border:1px solid #a9c9ef; font-weight:600; }"
            "QPushButton#secondaryAction:hover { background:#dfeeff; "
            "border-color:#7eafe8; }"
            "QScrollBar:vertical { background:transparent; width:10px; margin:2px; }"
            "QScrollBar::handle:vertical { background:#b8c6d6; border-radius:4px; "
            "min-height:32px; }"
            "QScrollBar::handle:vertical:hover { background:#91a4b8; }"
            "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical { height:0; }"
            "QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical { "
            "background:transparent; }"));
        setObjectName(QStringLiteral("launcherRoot"));

        auto* windowLayout = new QVBoxLayout(this);
        windowLayout->setContentsMargins(0, 0, 0, 0);
        auto* scroll = new QScrollArea(this);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setWidgetResizable(true);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto* content = new QWidget(scroll);
        content->setObjectName(QStringLiteral("launcherRoot"));
        auto* layout = new QVBoxLayout(content);
        layout->setContentsMargins(24, 16, 24, 12);
        layout->setSpacing(9);
        scroll->setWidget(content);
        windowLayout->addWidget(scroll);
        auto* title = new QLabel(QCoreApplication::translate("ASRTU", "阿斯图系列卫星接收与遥测上传"), this);
        QFont titleFont = title->font();
        titleFont.setPointSize(14);
        titleFont.setBold(true);
        title->setFont(titleFont);
        layout->addWidget(title);
        auto* subtitle = new QLabel(
            QCoreApplication::translate("ASRTU", "配置接收输入与地面站资料，再按需启动各个组件。"), this);
        subtitle->setObjectName(QStringLiteral("subtitle"));
        layout->addWidget(subtitle);

        auto* configurationCard = new QFrame(this);
        configurationCard->setObjectName(QStringLiteral("card"));
        configurationCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
        auto* configurationLayout = new QVBoxLayout(configurationCard);
        configurationLayout->setSizeConstraint(QLayout::SetMinimumSize);
        configurationLayout->setContentsMargins(18, 11, 18, 12);
        configurationLayout->setSpacing(7);
        auto* configurationTitle = new QLabel(QCoreApplication::translate("ASRTU", "接收与上传设置"), configurationCard);
        configurationTitle->setObjectName(QStringLiteral("sectionTitle"));
        configurationLayout->addWidget(configurationTitle);
        form_ = new QFormLayout;
        auto* form = form_;
        form->setContentsMargins(0, 0, 0, 0);
        form->setHorizontalSpacing(16);
        form->setVerticalSpacing(6);
        form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        nickname_ = new QLineEdit(this);
        nickname_->setMaxLength(32);
        longitude_ = makeCoordinate(-180.0, 180.0, 6);
        latitude_ = makeCoordinate(-90.0, 90.0, 6);
        altitude_ = makeCoordinate(-500.0, 10000.0, 2);
        altitude_->setSuffix(QStringLiteral(" m"));
        input_mode_ = new QComboBox(this);
        input_mode_->setSizeAdjustPolicy(
            QComboBox::AdjustToMinimumContentsLengthWithIcon);
        input_mode_->setMinimumContentsLength(18);
        const bool localBridgeAvailable =
            QFileInfo::exists(sdrSharpDirectory() + QStringLiteral("/SDRSharp.exe")) &&
            QFileInfo::exists(sdrSharpDirectory() +
                              QStringLiteral("/Plugins/SDRSharp.AstroSeriesBridge.dll"));
        if (localBridgeAvailable)
            input_mode_->addItem(
                QCoreApplication::translate("ASRTU", "本地内存共享 RAW 模式 I/Q 桥接"), 2);
        input_mode_->addItem(
            QCoreApplication::translate("ASRTU", "立体声零中频 RAW 模式 I/Q 输入"), 0);
        input_mode_->addItem(
            QCoreApplication::translate("ASRTU", "单声道实数域 12KHz 电台 IF 输入"), 1);
        audio_device_ = new QComboBox(this);
        audio_device_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        audio_device_->setMinimumContentsLength(18);
        refreshAudioDevices();
        recording_enabled_ = new QCheckBox(QCoreApplication::translate("ASRTU", "自动保存本次接收录音"), this);
        recording_enabled_->setChecked(true);
        satnogs_enabled_ = new QCheckBox(QCoreApplication::translate("ASRTU", "同时上传至 SatNOGS"), this);
        for (QWidget* field : {static_cast<QWidget*>(input_mode_),
                               static_cast<QWidget*>(audio_device_),
                               static_cast<QWidget*>(nickname_),
                               static_cast<QWidget*>(longitude_),
                               static_cast<QWidget*>(latitude_),
                               static_cast<QWidget*>(altitude_)}) {
            // QSS min-height alone is not included reliably in QFormLayout's
            // minimum size at fractional Windows DPI scaling.
            field->setMinimumHeight(30);
            field->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        }
        form->addRow(QCoreApplication::translate("ASRTU", "输入"), input_mode_);
        form->addRow(QCoreApplication::translate("ASRTU", "声卡"), audio_device_);
        form->addRow(QCoreApplication::translate("ASRTU", "呼号"), nickname_);
        form->addRow(QCoreApplication::translate("ASRTU", "经度"), longitude_);
        form->addRow(QCoreApplication::translate("ASRTU", "纬度"), latitude_);
        form->addRow(QCoreApplication::translate("ASRTU", "海拔"), altitude_);
        configurationLayout->addLayout(form);

        audio_device_label_ = form->labelForField(audio_device_);
        connect(input_mode_, qOverload<int>(&QComboBox::currentIndexChanged), this,
                [this](int) {
                    updateAudioDeviceVisibility();
                });
        auto* audioRefreshTimer = new QTimer(this);
        connect(audioRefreshTimer, &QTimer::timeout, this, [this] {
            if (input_mode_->currentData().toInt() != 2)
                refreshAudioDevices();
        });
        audioRefreshTimer->start(2000);

        layout->addWidget(configurationCard);

        auto* toolsCard = new QFrame(this);
        toolsCard->setObjectName(QStringLiteral("card"));
        auto* toolsLayout = new QVBoxLayout(toolsCard);
        toolsLayout->setContentsMargins(18, 10, 18, 11);
        toolsLayout->setSpacing(6);
        auto* toolsTitle = new QLabel(QCoreApplication::translate("ASRTU", "常用工具"), toolsCard);
        toolsTitle->setObjectName(QStringLiteral("sectionTitle"));
        toolsLayout->addWidget(toolsTitle);
        auto* tools = new QGridLayout;
        tools->setHorizontalSpacing(9);
        auto* sdrSharp = new QPushButton(QCoreApplication::translate("ASRTU", "打开 SDR# 遥测预设"), this);
        auto* tracker = new QPushButton(QCoreApplication::translate("ASRTU", "卫星跟踪与自动多普勒"), this);
        auto* playback = new QPushButton(QCoreApplication::translate("ASRTU", "播放录音文件"), this);
        auto* quickReplay = new QPushButton(QCoreApplication::translate("ASRTU", "快速播放录音文件"), this);
        auto* openRecords = new QPushButton(QCoreApplication::translate("ASRTU", "打开录音与日志"), this);
        for (auto* button : {sdrSharp, tracker, playback, quickReplay, openRecords}) {
            button->setObjectName(QStringLiteral("tool"));
            // Long translated captions must not increase the top-level
            // window's minimum width; the two equal grid columns provide
            // ample room at the compact launcher size.
            button->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
        }
        tools->addWidget(sdrSharp, 0, 0);
        tools->addWidget(tracker, 0, 1);
        tools->addWidget(playback, 1, 0);
        tools->addWidget(quickReplay, 1, 1);
        tools->addWidget(openRecords, 2, 0, 1, 2);
        toolsLayout->addLayout(tools);
        layout->addWidget(toolsCard);
        layout->addStretch(1);

        status_label_ = new QLabel(QCoreApplication::translate("ASRTU", "就绪"), this);
        status_label_->setStyleSheet(QStringLiteral(
            "color:#667788; padding:0 2px 2px 2px;"));
        layout->addWidget(status_label_);

        auto* buttons = new QHBoxLayout;
        buttons->setSpacing(9);
        auto* save = new QPushButton(QCoreApplication::translate("ASRTU", "保存设置"), this);
        auto* start = new QPushButton(QCoreApplication::translate("ASRTU", "启动接收"), this);
        auto* startProxyButton = new QPushButton(QCoreApplication::translate("ASRTU", "启动上传代理"), this);
        start->setObjectName(QStringLiteral("primary"));
        startProxyButton->setObjectName(QStringLiteral("secondaryAction"));
        buttons->addWidget(save, 0, Qt::AlignTop);
        buttons->addStretch(1);
        buttons->addWidget(startProxyButton, 0, Qt::AlignTop);
        auto* receiverActions = new QVBoxLayout;
        receiverActions->setSpacing(4);
        receiverActions->addWidget(start);
        receiverActions->addWidget(recording_enabled_, 0, Qt::AlignHCenter);
        receiverActions->addWidget(satnogs_enabled_, 0, Qt::AlignHCenter);
        buttons->addLayout(receiverActions);
        buttons->setAlignment(receiverActions, Qt::AlignTop);
        layout->addLayout(buttons);

        auto* credit = new QLabel(QStringLiteral("By BG7ZDQ"), this);
        credit->setAlignment(Qt::AlignRight);
        credit->setStyleSheet(QStringLiteral(
            "color:#7b8794; font-style:italic; padding-top:1px;"));
        layout->addWidget(credit);

        connect(save, &QPushButton::clicked, this, [this] { saveConfiguration(true); });
        connect(sdrSharp, &QPushButton::clicked, this, [this] {
            QString error;
            qint64 processId = 0;
            if (!startSdrSharp(&processId, &error))
                QMessageBox::critical(this, QCoreApplication::translate("ASRTU", "SDR# 启动失败"), error);
        });
        tracker->setProperty("satelliteTracker", true);
        connect(tracker, &QPushButton::clicked, this, [this] {
            auto* dialog = new SatelliteTrackerDialog(
                longitude_->value(), latitude_->value(), altitude_->value(),
                savedSatellite(), nullptr);
            dialog->show();
            dialog->raise();
            dialog->activateWindow();
        });
        connect(openRecords, &QPushButton::clicked, this, [this] {
            const QString directory = recordsDirectory();
            if (!QDir().mkpath(directory) ||
                !QDesktopServices::openUrl(QUrl::fromLocalFile(directory))) {
                QMessageBox::critical(this, QCoreApplication::translate("ASRTU", "无法打开目录"), directory);
            }
        });
        connect(playback, &QPushButton::clicked, this, [this] {
            const QString wavPath = QFileDialog::getOpenFileName(
                this, QCoreApplication::translate("ASRTU", "选择接收录音"), QString(),
                QCoreApplication::translate("ASRTU", "接收录音 (*.wav *.ogg *.oga *.opus *.flac *.mp3)"));
            if (wavPath.isEmpty())
                return;
            QString error;
            QString sessionDirectory;
            qint64 proxyProcessId = 0;
            if (!startSuite(false, 0, wavPath, -1, false,
                            false, {}, {}, 0.0, 0.0, &sessionDirectory,
                            &proxyProcessId, &error)) {
                QMessageBox::critical(this, QCoreApplication::translate("ASRTU", "播放失败"), error);
                return;
            }
            status_label_->setText(
                QCoreApplication::translate("ASRTU", "正在播放：/%1；日志：%2")
                    .arg(QFileInfo(wavPath).fileName(),
                         compactSessionDirectory(sessionDirectory)));
        });
        connect(quickReplay, &QPushButton::clicked, this, [this] {
            const QString initial = newestRecordingPath().isEmpty()
                                        ? recordsDirectory()
                                        : QFileInfo(newestRecordingPath()).absolutePath();
            const QString wavPath = QFileDialog::getOpenFileName(
                this, QCoreApplication::translate("ASRTU", "快速播放录音文件"), initial,
                QCoreApplication::translate("ASRTU", "接收录音 (*.wav *.ogg *.oga *.opus *.flac *.mp3)"));
            if (wavPath.isEmpty())
                return;
            QString error;
            QString sessionDirectory;
            qint64 proxyProcessId = 0;
            if (!startSuite(false, 0, wavPath, -1, false,
                            false, {}, {}, 0.0, 0.0, &sessionDirectory,
                            &proxyProcessId, &error)) {
                QMessageBox::critical(this, QCoreApplication::translate("ASRTU", "快速播放失败"), error);
                return;
            }
            status_label_->setText(
                QCoreApplication::translate("ASRTU", "正在重放：/%1/%2")
                    .arg(QFileInfo(QFileInfo(wavPath).absolutePath()).fileName(),
                         QFileInfo(wavPath).fileName()));
        });
        connect(startProxyButton, &QPushButton::clicked, this, [this] {
            bool accepted = false;
            const QString satellite = chooseSatellite(&accepted);
            if (!accepted || !saveConfiguration(false, satellite))
                return;
            QString error;
            qint64 processId = 0;
            if (!startProxy({}, &processId, &error)) {
                QMessageBox::critical(this, QCoreApplication::translate("ASRTU", "上传代理启动失败"), error);
                return;
            }
            status_label_->setText(
                QCoreApplication::translate("ASRTU", "上传代理已启动（PID %1）").arg(processId));
        });
        connect(start, &QPushButton::clicked, this, [this] {
            QString satnogsSatellite;
            if (satnogs_enabled_->isChecked()) {
                bool accepted = false;
                satnogsSatellite = chooseSatellite(&accepted);
                if (!accepted || !saveConfiguration(false, satnogsSatellite))
                    return;
            }
            QSettings settings(QStringLiteral("ASRTU"), QStringLiteral("AstroSeriesLauncher"));
            settings.setValue(QStringLiteral("input_mode"),
                              input_mode_->currentData().toInt());
            settings.setValue(QStringLiteral("recording_enabled"),
                              recording_enabled_->isChecked());
            settings.setValue(QStringLiteral("satnogs_enabled"),
                              satnogs_enabled_->isChecked());
            settings.setValue(QStringLiteral("audio_device_name"),
                              audio_device_->currentText());
            QString error;
            QString sessionDirectory;
            qint64 proxyProcessId = 0;
            if (!startSuite(false, input_mode_->currentData().toInt(), {},
                            audio_device_->currentData().toInt(),
                            recording_enabled_->isChecked(),
                            satnogs_enabled_->isChecked(), satnogsSatellite,
                            nickname_->text().trimmed(), longitude_->value(),
                            latitude_->value(),
                            &sessionDirectory, &proxyProcessId, &error)) {
                QMessageBox::critical(this, QCoreApplication::translate("ASRTU", "启动失败"), error);
                return;
            }
            status_label_->setText(satnogs_enabled_->isChecked()
                ? QCoreApplication::translate("ASRTU", "接收已启动；SatNOGS 上传已启用；录音与日志：%1")
                      .arg(compactSessionDirectory(sessionDirectory))
                : QCoreApplication::translate("ASRTU", "接收已启动；录音与日志：%1")
                      .arg(compactSessionDirectory(sessionDirectory)));
        });
        loadConfiguration();
        updateAudioDeviceVisibility();
        constexpr int preferredWidth = 620;
        constexpr int preferredHeight = 760;
        if (const QScreen* screen = QGuiApplication::primaryScreen()) {
            const QRect available = screen->availableGeometry();
            const double maximumArea = double(available.width()) * available.height() * 0.30;
            const double preferredArea = double(preferredWidth) * preferredHeight;
            const double scale = std::min(1.0, std::sqrt(maximumArea / preferredArea));
            const int targetWidth = int(std::floor(preferredWidth * scale));
            const int targetHeight = int(std::floor(preferredHeight * scale));
            setMinimumSize(std::min(minimumWidth(), targetWidth),
                           std::min(minimumHeight(), targetHeight));
            resize(targetWidth, targetHeight);
        } else {
            resize(preferredWidth, preferredHeight);
        }
    }

private:
    void updateAudioDeviceVisibility()
    {
        const bool visible = input_mode_->currentData().toInt() != 2;
        if (!form_ || !audio_device_label_)
            return;
        if (!visible && audio_row_inserted_) {
            const auto row = form_->takeRow(audio_device_);
            delete row.labelItem;
            delete row.fieldItem;
            audio_device_label_->hide();
            audio_device_->hide();
            audio_row_inserted_ = false;
        } else if (visible && !audio_row_inserted_) {
            audio_device_label_->show();
            audio_device_->show();
            form_->insertRow(1, audio_device_label_, audio_device_);
            audio_row_inserted_ = true;
        }
    }

    void refreshAudioDevices()
    {
        const QString previous = audio_device_ ? audio_device_->currentText() : QString();
        QList<QPair<QString, int>> devices;
        devices.append({QCoreApplication::translate("ASRTU", "系统默认输入设备"), -1});
#ifdef Q_OS_WIN
        const UINT count = waveInGetNumDevs();
        for (UINT id = 0; id < count; ++id) {
            WAVEINCAPSW caps{};
            if (waveInGetDevCapsW(id, &caps, sizeof(caps)) == MMSYSERR_NOERROR) {
                devices.append({QString::fromWCharArray(caps.szPname), int(id)});
            }
        }
#endif
        bool unchanged = audio_device_ && audio_device_->count() == devices.size();
        if (unchanged) {
            for (int i = 0; i < devices.size(); ++i) {
                if (audio_device_->itemText(i) != devices.at(i).first ||
                    audio_device_->itemData(i).toInt() != devices.at(i).second) {
                    unchanged = false;
                    break;
                }
            }
        }
        if (unchanged)
            return;

        audio_device_->clear();
        for (const auto& device : devices)
            audio_device_->addItem(device.first, device.second);
        QString wanted = previous;
        if (wanted.isEmpty()) {
            QSettings settings(QStringLiteral("ASRTU"), QStringLiteral("AstroSeriesLauncher"));
            wanted = settings.value(QStringLiteral("audio_device_name")).toString();
        }
        const int index = audio_device_->findText(wanted, Qt::MatchFixedString);
        audio_device_->setCurrentIndex(index >= 0 ? index : 0);
    }

    QString savedSatellite() const
    {
        QSettings settings(QStringLiteral("ASRTU"), QStringLiteral("AstroSeriesLauncher"));
        const QString saved = settings.value(QStringLiteral("satellite"),
                                             QStringLiteral("ASRTU-1")).toString();
        for (const auto& profile : satelliteProfiles()) {
            if (profile.name.compare(saved, Qt::CaseInsensitive) == 0)
                return profile.name;
        }
        return satelliteProfiles().first().name;
    }

    QString chooseSatellite(bool* accepted)
    {
        QStringList names;
        for (const auto& profile : satelliteProfiles())
            names.append(profile.name);
        const int current = std::max(0, names.indexOf(savedSatellite()));
        return QInputDialog::getItem(
            this, QCoreApplication::translate("ASRTU", "选择上传卫星"),
            QCoreApplication::translate("ASRTU", "本次遥测上传目标："), names, current, false, accepted);
    }

    QDoubleSpinBox* makeCoordinate(double minimum, double maximum, int decimals)
    {
        auto* value = new QDoubleSpinBox(this);
        value->setRange(minimum, maximum);
        value->setDecimals(decimals);
        value->setSingleStep(decimals > 2 ? 0.0001 : 1.0);
        value->setKeyboardTracking(false);
        return value;
    }

    void loadConfiguration()
    {
        QSettings settings(QStringLiteral("ASRTU"), QStringLiteral("AstroSeriesLauncher"));
        const int savedInputMode = settings.value(
            QStringLiteral("input_mode"), input_mode_->itemData(0)).toInt();
        const int savedInputIndex = input_mode_->findData(savedInputMode);
        input_mode_->setCurrentIndex(savedInputIndex >= 0 ? savedInputIndex : 0);
        recording_enabled_->setChecked(
            settings.value(QStringLiteral("recording_enabled"), true).toBool());
        satnogs_enabled_->setChecked(
            settings.value(QStringLiteral("satnogs_enabled"), false).toBool());
        const QString savedAudio = settings.value(
            QStringLiteral("audio_device_name"),
            QCoreApplication::translate("ASRTU", "系统默认输入设备")).toString();
        const int audioIndex = audio_device_->findText(savedAudio, Qt::MatchFixedString);
        audio_device_->setCurrentIndex(audioIndex >= 0 ? audioIndex : 0);
        updateAudioDeviceVisibility();
        QFile file(configPath());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return;
        const QString text = QString::fromUtf8(file.readAll());
        auto textValue = [&text](const QString& key) {
            const QRegularExpression expression(
                QStringLiteral("(?:^|\\n)\\s*%1\\s*=\\s*\"([^\"]*)\"\\s*;")
                    .arg(QRegularExpression::escape(key)));
            return expression.match(text).captured(1);
        };
        auto numberValue = [&text](const QString& key, double fallback) {
            const QRegularExpression expression(
                QStringLiteral("(?:^|\\n)\\s*%1\\s*=\\s*([-+0-9.eE]+)\\s*;")
                    .arg(QRegularExpression::escape(key)));
            const auto match = expression.match(text);
            bool ok = false;
            const double value = match.captured(1).toDouble(&ok);
            return ok ? value : fallback;
        };
        nickname_->setText(textValue(QStringLiteral("proxy_nickname")));
        const QString satellite = textValue(QStringLiteral("sat_name"));
        if (!satellite.isEmpty())
            settings.setValue(QStringLiteral("satellite"), satellite);
        longitude_->setValue(numberValue(QStringLiteral("proxy_long"), 0.0));
        latitude_->setValue(numberValue(QStringLiteral("proxy_lat"), 0.0));
        altitude_->setValue(numberValue(QStringLiteral("proxy_alt"), 0.0));
    }

    bool saveConfiguration(bool notify, QString satellite = {})
    {
        const QString nickname = nickname_->text().trimmed();
        if (nickname.isEmpty()) {
            QMessageBox::warning(this, QCoreApplication::translate("ASRTU", "资料不完整"),
                                 QCoreApplication::translate("ASRTU", "请填写呼号或昵称。"));
            nickname_->setFocus();
            return false;
        }
        QDir().mkpath(proxyDirectory());
        QSettings settings(QStringLiteral("ASRTU"), QStringLiteral("AstroSeriesLauncher"));
        if (satellite.isEmpty())
            satellite = savedSatellite();
        settings.setValue(QStringLiteral("satellite"), satellite);
        QSaveFile file(configPath());
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::critical(this, QCoreApplication::translate("ASRTU", "保存失败"),
                                  QCoreApplication::translate("ASRTU", "无法写入：\n%1").arg(configPath()));
            return false;
        }
        QTextStream stream(&file);
        stream.setCodec("UTF-8");
        const QString webSocket = webSocketForSatellite(satellite);
        stream << "# Server Config\n"
               << "zmq_address = \"tcp://127.0.0.1:5555\";\n"
               << "ws_address = \"" << webSocket << "\";\n"
               << "ws_port = 9000;\n\n"
               << "# Satellite Config\n"
               << "sat_name = \"" << satellite << "\";\n\n"
               << "# Proxy Config\n"
               << "physical_channel = 0;\n"
               << "proxy_nickname = \"" << escapedConfigString(nickname) << "\";\n"
               << "proxy_long = " << QString::number(longitude_->value(), 'f', 6) << ";\n"
               << "proxy_lat = " << QString::number(latitude_->value(), 'f', 6) << ";\n"
               << "proxy_alt = " << QString::number(altitude_->value(), 'f', 2) << ";\n";
        stream.flush();
        if (!file.commit()) {
            QMessageBox::critical(this, QCoreApplication::translate("ASRTU", "保存失败"),
                                  QCoreApplication::translate("ASRTU", "配置文件提交失败。"));
            return false;
        }
        if (notify && status_label_)
            status_label_->setText(QCoreApplication::translate("ASRTU", "设置已保存"));
        return true;
    }

    QLineEdit* nickname_ = nullptr;
    QDoubleSpinBox* longitude_ = nullptr;
    QDoubleSpinBox* latitude_ = nullptr;
    QDoubleSpinBox* altitude_ = nullptr;
    QComboBox* input_mode_ = nullptr;
    QComboBox* audio_device_ = nullptr;
    QWidget* audio_device_label_ = nullptr;
    QFormLayout* form_ = nullptr;
    bool audio_row_inserted_ = true;
    QCheckBox* recording_enabled_ = nullptr;
    QCheckBox* satnogs_enabled_ = nullptr;
    QLabel* status_label_ = nullptr;
};
}

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
    QFont uiFont(QStringLiteral("Microsoft YaHei UI"), 9);
    uiFont.setStyleStrategy(static_cast<QFont::StyleStrategy>(
        QFont::PreferAntialias | QFont::PreferQuality));
    uiFont.setHintingPreference(QFont::PreferFullHinting);
    application.setFont(uiFont);
    QCoreApplication::setOrganizationName(QStringLiteral("ASRTU"));
    QCoreApplication::setApplicationName(QStringLiteral("ASRTU1_Launcher"));

    if (application.arguments().contains(QStringLiteral("--start"))) {
        QString error;
        QString sessionDirectory;
        qint64 proxyProcessId = 0;
        if (!startSuite(true, 0, {}, -1, true,
                        false, {}, {}, 0.0, 0.0,
                        &sessionDirectory, &proxyProcessId, &error)) {
            QMessageBox::critical(nullptr, QCoreApplication::translate("ASRTU", "阿斯图系列卫星启动失败"), error);
            return 1;
        }
        return 0;
    }

    if (application.arguments().contains(QStringLiteral("--sdrsharp"))) {
        QString error;
        qint64 processId = 0;
        if (!startSdrSharp(&processId, &error)) {
            QMessageBox::critical(nullptr, QCoreApplication::translate("ASRTU", "SDR# 启动失败"), error);
            return 1;
        }
        return 0;
    }

    LauncherWindow window;
    window.show();
    const int trackerScreenshotOption =
        application.arguments().indexOf(QStringLiteral("--tracker-screenshot"));
    if (trackerScreenshotOption >= 0 &&
        trackerScreenshotOption + 1 < application.arguments().size()) {
        const QString path = application.arguments().at(trackerScreenshotOption + 1);
        QTimer::singleShot(200, &window, [&window] {
            const auto buttons = window.findChildren<QPushButton*>();
            for (QPushButton* button : buttons) {
                if (button->property("satelliteTracker").toBool()) {
                    button->click();
                    break;
                }
            }
        });
        QTimer::singleShot(9000, &window, [&application, path] {
            for (QWidget* widget : QApplication::topLevelWidgets()) {
                if (widget->windowTitle() == QCoreApplication::translate("ASRTU", "阿斯图系列卫星跟踪与多普勒")) {
                    widget->grab().save(path);
                    break;
                }
            }
            application.quit();
        });
    }
    const int screenshotOption = application.arguments().indexOf(QStringLiteral("--screenshot"));
    if (screenshotOption >= 0 && screenshotOption + 1 < application.arguments().size()) {
        const QString path = application.arguments().at(screenshotOption + 1);
        QTimer::singleShot(300, &window, [&application, &window, path] {
            window.grab().save(path);
            application.quit();
        });
    }
    return application.exec();
}
