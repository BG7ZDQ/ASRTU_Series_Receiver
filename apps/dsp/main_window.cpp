#include "main_window.h"

#include "flowgraph.h"
#include "rssi_meter.h"
#include "snr_plot.h"
#include "ssdv_image_window.h"
#include "ssdv_receiver.h"

#include <QApplication>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>
#include <QMetaObject>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QResizeEvent>
#include <QSettings>
#include <QScreen>
#include <QSplitter>
#include <QTextStream>
#include <QTimer>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QWidget>

#include <qwt_plot.h>
#include <qwt_text.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>

namespace {
class RightClickResetFilter final : public QObject
{
public:
    RightClickResetFilter(std::function<void()> reset, QObject* parent)
        : QObject(parent), reset_(std::move(reset)) {}

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::MouseButtonPress) {
            auto* mouse = static_cast<QMouseEvent*>(event);
            if (mouse->button() == Qt::RightButton) {
                reset_();
                event->accept();
                return true;
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    std::function<void()> reset_;
};

class EqualAxisScaleFilter final : public QObject
{
public:
    EqualAxisScaleFilter(QwtPlot* plot, QObject* parent)
        : QObject(parent), plot_(plot)
    {
        if (plot_ && plot_->canvas())
            plot_->canvas()->installEventFilter(this);
    }

    void synchronize()
    {
        if (!plot_ || !plot_->canvas() || updating_)
            return;
        const int width = plot_->canvas()->width();
        const int height = plot_->canvas()->height();
        if (width < 2 || height < 2)
            return;

        updating_ = true;
        const double aspect = double(width) / double(height);
        const double xHalfRange = aspect >= 1.0 ? 2.0 * aspect : 2.0;
        const double yHalfRange = aspect >= 1.0 ? 2.0 : 2.0 / aspect;
        plot_->setAxisScale(QwtPlot::xBottom, -xHalfRange, xHalfRange);
        plot_->setAxisScale(QwtPlot::yLeft, -yHalfRange, yHalfRange);
        plot_->replot();
        updating_ = false;
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (plot_ && watched == plot_->canvas() &&
            (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
            synchronize();
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QwtPlot* plot_ = nullptr;
    bool updating_ = false;
};

class ResponsivePlotSplitter final : public QSplitter
{
public:
    explicit ResponsivePlotSplitter(QWidget* parent = nullptr)
        : QSplitter(Qt::Vertical, parent)
    {
    }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QSplitter::resizeEvent(event);
        const int available = height() - handleWidth();
        if (available <= 0 || count() < 2)
            return;
        // The Qwt constellation widget needs roughly the right-column width
        // as its outer height for the inner -2..2 canvas to appear square.
        const int maxConstellationHeight =
            std::max(300, std::min(430, available - 150));
        const int constellationHeight = std::clamp(
            width() - 20, 300, maxConstellationHeight);
        setSizes({constellationHeight, available - constellationHeight});
    }
};

class ResponsiveMainSplitter final : public QSplitter
{
public:
    explicit ResponsiveMainSplitter(QWidget* parent = nullptr)
        : QSplitter(Qt::Horizontal, parent)
    {
    }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QSplitter::resizeEvent(event);
        const int available = width() - handleWidth();
        if (available <= 0 || count() < 2)
            return;

        // Keep the demodulator column close to the GNU Radio proportions.
        // On a normal window it occupies about 48%; on a wide/full-screen
        // window it stops growing at 520 px and the plots receive the rest.
        const int maxRightWidth = std::max(420, std::min(520, available - 300));
        const int rightWidth = std::clamp(
            static_cast<int>(std::lround(width() * 0.48)), 420, maxRightWidth);
        setSizes({available - rightWidth, rightWidth});
    }
};

void installRightClickReset(QWidget* widget,
                            QObject* owner,
                            std::function<void()> reset)
{
    auto* filter = new RightClickResetFilter(std::move(reset), owner);
    widget->installEventFilter(filter);
    const auto descendants = widget->findChildren<QObject*>();
    for (auto* child : descendants)
        child->installEventFilter(filter);
    widget->setToolTip(QStringLiteral("Right-click to restore the default view"));
}

void reducePlotTitleFont(QWidget* widget)
{
    QList<QObject*> candidates = widget->findChildren<QObject*>();
    candidates.prepend(widget);
    for (auto* candidate : candidates) {
        if (!candidate->inherits("QwtPlot"))
            continue;
        auto* plot = static_cast<QwtPlot*>(candidate);
        QwtText title = plot->title();
        QFont titleFont = title.font();
        titleFont.setPointSize(9);
        title.setFont(titleFont);
        plot->setTitle(title);
    }
}

QString requestedSessionDirectory()
{
    const QStringList arguments = QCoreApplication::arguments();
    const int option = arguments.indexOf(QStringLiteral("--session-dir"));
    if (option >= 0 && option + 1 < arguments.size())
        return QDir::cleanPath(arguments.at(option + 1));
    return {};
}

QString createSessionDirectory()
{
    QString directory = requestedSessionDirectory();
    if (directory.isEmpty()) {
        const QString root = QDir::cleanPath(
            QDir(QCoreApplication::applicationDirPath())
                .absoluteFilePath(QStringLiteral("../ASRTU1_Records")));
        directory = QDir(root).filePath(
            QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz")));
    }
    if (!QDir().mkpath(directory))
        throw std::runtime_error(
            QStringLiteral("Unable to create session directory: %1")
                .arg(directory).toUtf8().constData());
    return QDir(directory).absolutePath();
}

bool requestedRealIf12k()
{
    return QCoreApplication::arguments().contains(QStringLiteral("--real-if-12k"));
}

bool requestedSharedIqBridge()
{
    return QCoreApplication::arguments().contains(
        QStringLiteral("--sdrsharp-iq-bridge"));
}

int requestedAudioDevice()
{
    const QStringList arguments = QCoreApplication::arguments();
    const int option = arguments.indexOf(QStringLiteral("--audio-device"));
    if (option < 0 || option + 1 >= arguments.size())
        return -1;
    bool valid = false;
    const int device = arguments.at(option + 1).toInt(&valid);
    return valid ? device : -1;
}

bool requestedRecordingEnabled()
{
    return !QCoreApplication::arguments().contains(QStringLiteral("--no-record"));
}

QString requestedWavPath()
{
    const QStringList arguments = QCoreApplication::arguments();
    const int option = arguments.indexOf(QStringLiteral("--wav"));
    if (option >= 0 && option + 1 < arguments.size())
        return QDir::cleanPath(arguments.at(option + 1));
    return {};
}

bool requestedFastPlayback()
{
    return QCoreApplication::arguments().contains(
        QStringLiteral("--fast-playback"));
}

double requestedReplayRate()
{
    const QStringList arguments = QCoreApplication::arguments();
    const int option = arguments.indexOf(QStringLiteral("--replay-rate"));
    if (option >= 0 && option + 1 < arguments.size())
        return std::clamp(arguments.at(option + 1).toDouble(), 2.0, 100.0);
    return 10.0;
}

QString requestedOption(const QString& name)
{
    const QStringList arguments = QCoreApplication::arguments();
    const int option = arguments.indexOf(name);
    if (option >= 0 && option + 1 < arguments.size())
        return arguments.at(option + 1);
    return {};
}

QString compactSessionPath(const QString& sessionDirectory, const QString& path)
{
    return QStringLiteral("/%1/%2")
        .arg(QFileInfo(sessionDirectory).fileName(), QFileInfo(path).fileName());
}
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    setWindowTitle(QCoreApplication::translate("ASRTU", "阿斯图系列卫星接收解码"));
    setMinimumSize(760, 640);
    if (const QScreen* screen = QGuiApplication::primaryScreen()) {
        const QSize available = screen->availableGeometry().size();
        resize(std::clamp(int(available.width() * 0.72), 860, 1360),
               std::clamp(int(available.height() * 0.90), 720, 1080));
    } else {
        resize(960, 900);
    }

    session_directory_ = createSessionDirectory();
    playback_path_ = requestedWavPath();
    fast_playback_ = !playback_path_.isEmpty() && requestedFastPlayback();
    real_if_12khz_ = requestedRealIf12k();
    shared_iq_bridge_ = requestedSharedIqBridge();
    audio_device_id_ = requestedAudioDevice();
    recording_enabled_ = playback_path_.isEmpty() && requestedRecordingEnabled();
    const QString logPath = QDir(session_directory_).filePath(
        QStringLiteral("decoder.log"));
    if (recording_enabled_)
        recording_path_ = QDir(session_directory_).filePath(
            QStringLiteral("recording.wav"));

    // GNU Radio 3.10's Windows WAV sink ultimately opens a narrow-character
    // filename.  Passing UTF-8 here breaks whenever the install/session path
    // contains Chinese characters.  QFile::encodeName uses the Windows local
    // filename encoding expected by that API.
    if (!recording_path_.isEmpty()) {
        QFile recordingProbe(recording_path_);
        if (!recordingProbe.open(QIODevice::WriteOnly))
            throw std::runtime_error(
                QStringLiteral("Unable to create WAV file: %1\n%2")
                    .arg(recording_path_, recordingProbe.errorString())
                    .toUtf8().constData());
        recordingProbe.close();
        recordingProbe.remove();
    }
    log_file_.setFileName(logPath);
    if (!log_file_.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        throw std::runtime_error(
            QStringLiteral("Unable to open log file: %1")
                .arg(logPath).toUtf8().constData());

    try {
        flowgraph_ = createFlowgraph(audio_device_id_, recording_path_);
        buildUi();
        ssdv_window_ = new SsdvImageWindow(this);
        ssdv_receiver_ = std::make_unique<SsdvReceiver>(
            session_directory_,
            [this](const SsdvImageUpdate& update) {
                if (ssdv_window_)
                    ssdv_window_->updateImage(update);
            },
            [this](const QString& line) { appendLog(line); });
        ssdv_window_->setClearCallback([this] {
            if (ssdv_receiver_)
                ssdv_receiver_->clear();
            appendLog(QStringLiteral("SSDV image display cleared"));
        });
        appendLog(QStringLiteral("C++/Qt demodulator initialized"));
        appendLog(QStringLiteral("TCP PDU: 127.0.0.1:9985; ZMQ PUB: 127.0.0.1:5555"));
        appendLog(QStringLiteral("Log file: %1")
                      .arg(compactSessionPath(session_directory_, logPath)));
        if (!playback_path_.isEmpty()) {
            appendLog(QStringLiteral("Playback file: /%1")
                          .arg(QFileInfo(playback_path_).fileName()));
            appendLog(fast_playback_
                          ? QStringLiteral("Playback speed: %1x (fast replay)").arg(requestedReplayRate(), 0, 'f', 1)
                          : QStringLiteral("Playback speed: real-time (1x)"));
        } else if (!recording_path_.isEmpty()) {
            appendLog(QStringLiteral("Automatic WAV recording: %1")
                          .arg(compactSessionPath(session_directory_, recording_path_)));
        } else {
            appendLog(QStringLiteral("Automatic WAV recording: disabled"));
        }
        appendLog(!playback_path_.isEmpty()
                      ? (real_if_12khz_
                             ? QStringLiteral("Input mode: WAV mono real IF centered at +12 kHz")
                             : QStringLiteral("Input mode: WAV stereo zero-IF I/Q"))
                      : shared_iq_bridge_
                      ? QStringLiteral("Input mode: SDRSharp local RAW I/Q bridge")
                      : real_if_12khz_
                            ? QStringLiteral("Input mode: real mono IF centered at +12 kHz")
                            : QStringLiteral("Input mode: stereo zero-IF I/Q"));
        if (playback_path_.isEmpty() && !shared_iq_bridge_)
            appendLog(QStringLiteral("Audio input device ID: %1")
                          .arg(audio_device_id_));
        flowgraph_->start();
        appendLog(QStringLiteral("Flowgraph started"));
    } catch (const std::exception& e) {
        QMessageBox::critical(this, QStringLiteral("Initialization failed"),
                              QString::fromUtf8(e.what()));
        throw;
    }

    status_timer_ = new QTimer(this);
    connect(status_timer_, &QTimer::timeout, this, &MainWindow::updateStatus);
    status_timer_->start(200);
    snr_log_timer_.start();
    updateStatus();
    setupControlServer();

    QSettings settings(QStringLiteral("ASRTU"), QStringLiteral("ASRTU1_Demod_CQt_v3"));
    const auto geometry = settings.value(QStringLiteral("geometry")).toByteArray();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
        bool visible = false;
        for (const QScreen* screen : QGuiApplication::screens()) {
            if (screen->availableGeometry().intersects(frameGeometry())) {
                visible = true;
                break;
            }
        }
        if (!visible)
            move(QGuiApplication::primaryScreen()->availableGeometry().topLeft() +
                 QPoint(30, 30));
    }
}

MainWindow::~MainWindow()
{
    if (flowgraph_)
        flowgraph_->stop();
    log_file_.close();
}

void MainWindow::buildUi()
{
    auto* central = new QWidget(this);
    central->setObjectName(QStringLiteral("decoderRoot"));
    central->setStyleSheet(QStringLiteral(
        "QWidget#decoderRoot { background:#f7faff; color:#17202a; }"
        "QFrame#statusCard { background:#ffffff; border:1px solid #d9e5f2; "
        "border-radius:8px; }"
        "QLabel#metric { color:#243447; font-weight:600; }"
        "QDoubleSpinBox { min-height:28px; padding:0 7px; background:#ffffff; "
        "border:1px solid #cbd5e1; border-radius:5px; }"
        "QSlider::groove:horizontal { height:5px; background:#dbe3eb; "
        "border-radius:2px; }"
        "QSlider::sub-page:horizontal { background:#2b7de9; border-radius:2px; }"
        "QSlider::handle:horizontal { width:15px; margin:-5px 0; "
        "background:#ffffff; border:2px solid #2b7de9; border-radius:7px; }"
        "QSplitter::handle { background:#f7faff; }"
        "QSplitter::handle:horizontal { width:8px; }"
        "QSplitter::handle:vertical { height:8px; }"));
    auto* outerLayout = new QVBoxLayout(central);
    outerLayout->setContentsMargins(10, 10, 10, 10);
    outerLayout->setSpacing(0);

    auto* mainSplitter = new ResponsiveMainSplitter(central);
    mainSplitter->setChildrenCollapsible(false);
    mainSplitter->setOpaqueResize(false);
    mainSplitter->setHandleWidth(8);
    outerLayout->addWidget(mainSplitter);

    auto* leftSplitter = new QSplitter(Qt::Vertical, mainSplitter);
    leftSplitter->setChildrenCollapsible(false);
    leftSplitter->setOpaqueResize(false);
    leftSplitter->setHandleWidth(8);

    auto* demod = new QWidget(mainSplitter);
    demod->setObjectName(QStringLiteral("demodPanel"));
    demod->setMinimumWidth(390);
    demod->setStyleSheet(QStringLiteral(
        "QWidget#demodPanel { background:#ffffff; border:1px solid #d9e5f2; "
        "border-radius:7px; }"));
    auto* demodLayout = new QVBoxLayout(demod);
    demodLayout->setContentsMargins(4, 4, 4, 4);
    demodLayout->setSpacing(3);

    auto* statusCard = new QFrame(demod);
    statusCard->setObjectName(QStringLiteral("statusCard"));
    auto* status = new QHBoxLayout(statusCard);
    status->setContentsMargins(12, 7, 9, 7);
    status->setSpacing(8);
    QFont infoFont = font();
    infoFont.setPointSize(10);
    infoFont.setBold(true);
    infoFont.setStyleStrategy(static_cast<QFont::StyleStrategy>(
        QFont::PreferAntialias | QFont::PreferQuality));
    snr_label_ = new QLabel(QStringLiteral("SNR: -- dB"), demod);
    frequency_label_ = new QLabel(QStringLiteral("Loop df: -- Hz"), demod);
    sync_label_ = new QLabel(QStringLiteral("NOSYNC"), demod);
    for (auto* label : { snr_label_, frequency_label_, sync_label_ }) {
        label->setFont(infoFont);
        label->setObjectName(QStringLiteral("metric"));
    }
    sync_label_->setAlignment(Qt::AlignCenter);
    sync_label_->setMinimumWidth(115);
    setSyncDisplay(false);

    status->addWidget(snr_label_);
    status->addWidget(frequency_label_);
    status->addStretch(1);
    status->addWidget(sync_label_);
    demodLayout->addWidget(statusCard);
    snr_plot_ = new SnrPlot(demod);
    snr_plot_->setMaximumHeight(130);
    demodLayout->addWidget(snr_plot_);

    rssi_meter_ = new RssiMeter(demod);
    auto* constellation = flowgraph_->constellationWidget();
    constellation->setMinimumHeight(300);
    constellation->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QwtPlot* constellationPlot = dynamic_cast<QwtPlot*>(constellation);
    if (!constellationPlot) {
        for (QWidget* child : constellation->findChildren<QWidget*>()) {
            if ((constellationPlot = dynamic_cast<QwtPlot*>(child)))
                break;
        }
    }
    auto* equalAxisScale = constellationPlot
                               ? new EqualAxisScaleFilter(constellationPlot, this)
                               : nullptr;
    auto* loopSpectrum = flowgraph_->loopSpectrumWidget();
    loopSpectrum->setMinimumHeight(150);
    demodLayout->addWidget(rssi_meter_);
    auto* demodPlots = new ResponsivePlotSplitter(demod);
    demodPlots->setChildrenCollapsible(false);
    demodPlots->setOpaqueResize(false);
    demodPlots->setHandleWidth(7);
    demodPlots->addWidget(constellation);
    demodPlots->addWidget(loopSpectrum);
    demodPlots->setStretchFactor(0, 4);
    demodPlots->setStretchFactor(1, 1);
    demodPlots->setSizes({420, 300});
    demodLayout->addWidget(demodPlots, 1);
    auto* waterfall = flowgraph_->waterfallWidget();
    auto* inputSpectrum = flowgraph_->inputSpectrumWidget();
    for (auto* plotWidget : { waterfall, inputSpectrum, constellation, loopSpectrum })
        reducePlotTitleFont(plotWidget);
    leftSplitter->addWidget(waterfall);
    leftSplitter->addWidget(inputSpectrum);
    leftSplitter->setStretchFactor(0, 1);
    leftSplitter->setStretchFactor(1, 1);
    leftSplitter->setSizes({500, 500});
    mainSplitter->addWidget(leftSplitter);
    mainSplitter->addWidget(demod);
    mainSplitter->setStretchFactor(0, 11);
    mainSplitter->setStretchFactor(1, 10);
    mainSplitter->setSizes({520, 480});

    installRightClickReset(inputSpectrum, this,
                           [this] { flowgraph_->resetInputSpectrum(); });
    installRightClickReset(waterfall, this,
                           [this] { flowgraph_->resetWaterfall(); });
    installRightClickReset(constellation, this,
                           [this, equalAxisScale] {
                               flowgraph_->resetConstellation();
                               if (equalAxisScale)
                                   equalAxisScale->synchronize();
                           });
    installRightClickReset(loopSpectrum, this,
                           [this] { flowgraph_->resetLoopSpectrum(); });
    setCentralWidget(central);
    if (equalAxisScale) {
        QTimer::singleShot(0, this,
                           [equalAxisScale] { equalAxisScale->synchronize(); });
    }
}

void MainWindow::updateStatus()
{
    if (!flowgraph_ || !flowgraph_->running()) {
        setSyncDisplay(false);
        return;
    }
    try {
        if (!iq_mismatch_warned_ && flowgraph_->stereoIqContentMismatch()) {
            if (++iq_mismatch_ticks_ >= 5) {
                iq_mismatch_warned_ = true;
                QMessageBox::warning(
                    this,
                    QCoreApplication::translate("ASRTU", "I/Q 输入异常"),
                    QCoreApplication::translate(
                        "ASRTU",
                        "检测到 I/Q 两路幅度严重不平衡，当前输入可能是单声道 USB/实数音频，因此频谱会出现镜像。\n请在启动器中改选“单声道实数域 12KHz 电台 IF 输入”。"));
            }
        } else if (!iq_mismatch_warned_) {
            iq_mismatch_ticks_ = 0;
        }
        if (!flowgraph_->inputActive(0.5)) {
            snr_plot_->addGap();
            snr_label_->setText(QStringLiteral("SNR: -- dB"));
            frequency_label_->setText(QStringLiteral("Loop df: -- Hz"));
            setSyncDisplay(false);
            return;
        }
        const double snr = flowgraph_->snr();
        if (std::isfinite(snr)) {
            snr_plot_->addValue(snr); // starts immediately, independent of frames
            snr_label_->setText(QStringLiteral("SNR: %1 dB").arg(snr, 0, 'f', 2));
            const qint64 elapsed = snr_log_timer_.elapsed();
            if (elapsed - last_snr_log_ms_ >= 1000) {
                appendLog(QStringLiteral("SVR SNR: %1 dB").arg(snr, 0, 'f', 6));
                last_snr_log_ms_ = elapsed;
            }
        }
        frequency_label_->setText(QStringLiteral("Loop df: %1 Hz")
                                      .arg(flowgraph_->loopFrequencyHz(), 0, 'f', 1));
        const double rssi = flowgraph_->rssi();
        if (std::isfinite(rssi)) {
            const double targetMin = std::floor((rssi - 10.0) / 5.0) * 5.0;
            const double targetMax = std::ceil((rssi + 15.0) / 5.0) * 5.0;
            if (!rssi_range_ready_) {
                rssi_min_ = targetMin;
                rssi_max_ = targetMax;
                rssi_range_ready_ = true;
            } else {
                rssi_min_ = targetMin < rssi_min_
                                ? targetMin
                                : rssi_min_ * 0.65 + targetMin * 0.35;
                rssi_max_ = targetMax > rssi_max_
                                ? targetMax
                                : rssi_max_ * 0.65 + targetMax * 0.35;
            }
            rssi_meter_->setReading(rssi, rssi_min_, rssi_max_);
        }
        setSyncDisplay(flowgraph_->synced(1.5));
    } catch (const std::exception& e) {
        appendLog(QStringLiteral("Status read failed: %1").arg(QString::fromUtf8(e.what())));
    }
}

void MainWindow::setSyncDisplay(bool synced)
{
    sync_label_->setText(synced ? QStringLiteral("SYNCED") : QStringLiteral("NOSYNC"));
    sync_label_->setStyleSheet(QStringLiteral(
        "QLabel { color:white; background:%1; border-radius:3px; padding:3px 10px; }")
        .arg(synced ? QStringLiteral("#159447") : QStringLiteral("#b43b3b")));
}

void MainWindow::appendLog(const QString& text)
{
    const QString line = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz ")) + text;
    if (log_file_.isOpen()) {
        QTextStream stream(&log_file_);
        stream.setCodec("UTF-8");
        stream << line << '\n';
        stream.flush();
    }
}

std::unique_ptr<AsrtuFlowgraph> MainWindow::createFlowgraph(
    int deviceId, const QString& recordingPath)
{
    AsrtuFlowgraph::Options options;
    if (!playback_path_.isEmpty())
        options.wav_path = QFile::encodeName(playback_path_).constData();
    if (!recordingPath.isEmpty())
        options.record_wav_path = QFile::encodeName(recordingPath).constData();
    options.real_if_12khz = real_if_12khz_;
    options.shared_iq_bridge = shared_iq_bridge_;
    options.fast_playback = fast_playback_;
    options.replay_rate = requestedReplayRate();
    options.audio_device_id = deviceId;
    options.payload_callback = [this](const std::vector<std::uint8_t>& payload) {
        const QByteArray frame(reinterpret_cast<const char*>(payload.data()),
                               int(payload.size()));
        QMetaObject::invokeMethod(
            this, [this, frame] { handleDecodedFrame(frame); },
            Qt::QueuedConnection);
    };
    options.local_candidate_callback = [this](const std::vector<std::uint8_t>& payload) {
        const QByteArray frame(reinterpret_cast<const char*>(payload.data()),
                               int(payload.size()));
        QMetaObject::invokeMethod(this, [this, frame] {
            if (ssdv_receiver_)
                ssdv_receiver_->ingestFrame(frame);
        }, Qt::QueuedConnection);
    };
    if (options.real_if_12khz)
        options.input_frequency_hz = -12000.0;
    return std::make_unique<AsrtuFlowgraph>([this](const std::string& line) {
        const QString text = QString::fromStdString(line);
        QMetaObject::invokeMethod(this, [this, text] { appendLog(text); },
                                  Qt::QueuedConnection);
    }, options);
}

void MainWindow::handleDecodedFrame(const QByteArray& frame)
{
    if (ssdv_receiver_)
        ssdv_receiver_->ingestFrame(frame);
    // Replayed recordings may be decoded locally, but must never be uploaded
    // again because telemetry frames do not contain a trustworthy timestamp.
    if (playback_path_.isEmpty())
        submitSatnogsFrame(frame);
}

void MainWindow::setupControlServer()
{
    control_server_ = new QLocalServer(this);
    const QString name = QStringLiteral("ASRTU_DSP_CONTROL_V1");
    if (!control_server_->listen(name)) {
#ifdef Q_OS_UNIX
        QLocalServer::removeServer(name);
        if (!control_server_->listen(name)) {
            appendLog(QStringLiteral("DSP control channel unavailable: %1")
                          .arg(control_server_->errorString()));
            return;
        }
#else
        appendLog(QStringLiteral("DSP control channel unavailable: %1")
                      .arg(control_server_->errorString()));
        return;
#endif
    }
    connect(control_server_, &QLocalServer::newConnection, this, [this] {
        while (control_server_->hasPendingConnections()) {
            QLocalSocket* socket = control_server_->nextPendingConnection();
            connect(socket, &QLocalSocket::readyRead, this,
                    [this, socket] { handleControlSocket(socket); });
            connect(socket, &QLocalSocket::disconnected,
                    socket, &QObject::deleteLater);
            if (socket->bytesAvailable() > 0)
                handleControlSocket(socket);
        }
    });
}

void MainWindow::handleControlSocket(QLocalSocket* socket)
{
    const QByteArray command = socket->readAll().trimmed();
    constexpr char prefix[] = "audio-device:";
    if (!command.startsWith(prefix))
        return;
    bool valid = false;
    const int deviceId = command.mid(int(sizeof(prefix) - 1)).toInt(&valid);
    if (valid)
        switchAudioDevice(deviceId);
}

void MainWindow::switchAudioDevice(int deviceId)
{
    if (!playback_path_.isEmpty() || shared_iq_bridge_ ||
        (deviceId == audio_device_id_ && flowgraph_))
        return;

    appendLog(QStringLiteral("Switching audio input device: %1 -> %2")
                  .arg(audio_device_id_).arg(deviceId));
    status_timer_->stop();
    QWidget* oldCentral = takeCentralWidget();
    const int oldDeviceId = audio_device_id_;
    const QString oldRecordingPath = recording_path_;
    try {
        if (flowgraph_)
            flowgraph_->stop();
        flowgraph_.reset();
        delete oldCentral;
        oldCentral = nullptr;

        if (recording_enabled_) {
            ++recording_segment_;
            recording_path_ = QDir(session_directory_).filePath(
                QStringLiteral("recording_part%1.wav")
                    .arg(recording_segment_, 2, 10, QLatin1Char('0')));
        }
        flowgraph_ = createFlowgraph(deviceId, recording_path_);
        audio_device_id_ = deviceId;
        buildUi();
        flowgraph_->start();
        appendLog(QStringLiteral("Audio input device switched to ID %1")
                      .arg(deviceId));
        if (recording_enabled_)
            appendLog(QStringLiteral("Automatic WAV recording continued: %1")
                          .arg(compactSessionPath(session_directory_, recording_path_)));
    } catch (const std::exception& error) {
        delete oldCentral;
        flowgraph_.reset();
        recording_path_ = oldRecordingPath;
        audio_device_id_ = oldDeviceId;
        appendLog(QStringLiteral("Audio device switch failed: %1")
                      .arg(QString::fromUtf8(error.what())));
        QMessageBox::warning(
            this, QCoreApplication::translate("ASRTU", "声卡切换失败"),
            QCoreApplication::translate(
                "ASRTU", "无法切换到所选声卡。请重新选择可用设备或重新启动接收。\n%1")
                .arg(QString::fromUtf8(error.what())));
        try {
            flowgraph_ = createFlowgraph(oldDeviceId, recording_path_);
            buildUi();
            flowgraph_->start();
            appendLog(QStringLiteral("Previous audio input restored"));
        } catch (const std::exception& restoreError) {
            flowgraph_.reset();
            auto* unavailable = new QWidget(this);
            auto* layout = new QVBoxLayout(unavailable);
            auto* message = new QLabel(
                QCoreApplication::translate(
                    "ASRTU",
                    "当前声卡不可用。请在启动器中选择另一输入设备。\n%1")
                    .arg(QString::fromUtf8(restoreError.what())),
                unavailable);
            message->setAlignment(Qt::AlignCenter);
            message->setWordWrap(true);
            layout->addWidget(message);
            setCentralWidget(unavailable);
            appendLog(QStringLiteral("Previous audio input could not be restored: %1")
                          .arg(QString::fromUtf8(restoreError.what())));
        }
    }
    rssi_range_ready_ = false;
    iq_mismatch_ticks_ = 0;
    iq_mismatch_warned_ = false;
    status_timer_->start(200);
    updateStatus();
}

void MainWindow::submitSatnogsFrame(const QByteArray& frame)
{
    if (frame.isEmpty())
        return;

    QSettings settings(QStringLiteral("ASRTU"), QStringLiteral("AstroSeriesLauncher"));
    if (!settings.value(QStringLiteral("satnogs_enabled"), false).toBool())
        return;
    const int noradId = settings.value(QStringLiteral("satnogs_norad_id"), 0).toInt();
    const QString source = settings.value(QStringLiteral("satnogs_source")).toString().trimmed();
    const double longitude = settings.value(QStringLiteral("satnogs_longitude"), 0.0).toDouble();
    const double latitude = settings.value(QStringLiteral("satnogs_latitude"), 0.0).toDouble();
    if (noradId <= 0 || source.isEmpty())
        return;
    if (!satnogs_network_)
        satnogs_network_ = new QNetworkAccessManager(this);

    const auto coordinate = [](double value, QChar positive, QChar negative) {
        return QStringLiteral("%1%2")
            .arg(std::abs(value), 0, 'f', 6)
            .arg(value >= 0.0 ? positive : negative);
    };
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("noradID"),
                      QString::number(noradId));
    form.addQueryItem(QStringLiteral("source"), source);
    form.addQueryItem(QStringLiteral("locator"), QStringLiteral("longLat"));
    form.addQueryItem(QStringLiteral("longitude"),
                      coordinate(longitude, QLatin1Char('E'),
                                 QLatin1Char('W')));
    form.addQueryItem(QStringLiteral("latitude"),
                      coordinate(latitude, QLatin1Char('N'),
                                 QLatin1Char('S')));
    form.addQueryItem(QStringLiteral("version"), QStringLiteral("1.6.6"));
    form.addQueryItem(QStringLiteral("frame"),
                      QString::fromLatin1(frame.toHex().toUpper()));
    form.addQueryItem(QStringLiteral("timestamp"),
                      QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));

    QUrl endpoint(QStringLiteral("https://db.satnogs.org/api/telemetry/"));
    endpoint.setQuery(form);
    QNetworkRequest request(endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
    QNetworkReply* reply = satnogs_network_->post(
        request, form.query(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        const int status = reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() == QNetworkReply::NoError) {
            appendLog(QStringLiteral("SatNOGS upload accepted (HTTP %1)")
                          .arg(status));
        } else {
            appendLog(QStringLiteral("SatNOGS upload failed (HTTP %1): %2")
                          .arg(status)
                          .arg(reply->errorString()));
        }
        reply->deleteLater();
    });
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (status_timer_)
        status_timer_->stop();
    if (flowgraph_)
        flowgraph_->stop();
    QSettings settings(QStringLiteral("ASRTU"), QStringLiteral("ASRTU1_Demod_CQt_v3"));
    settings.setValue(QStringLiteral("geometry"), saveGeometry());
    event->accept();
}
