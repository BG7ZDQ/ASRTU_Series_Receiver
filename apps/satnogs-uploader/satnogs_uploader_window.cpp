#include "satnogs_uploader_window.h"

#include "pmt_frame_decoder.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFontDatabase>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPlainTextEdit>
#include <QPointer>
#include <QScrollBar>
#include <QSplitter>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QWidget>
#include <QtGlobal>
#include <zmq.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace asrtu
{
namespace
{
constexpr int kReceiveIntervalMs = 10;
constexpr int kMaximumFramesPerTick = 64;
constexpr int kMaximumPendingFrames = 128;
constexpr int kMaximumInFlightRequests = 4;
constexpr int kMaximumResponseCharacters = 2048;
constexpr int kRequestTimeoutMs = 15000;
constexpr int kMaximumFeedbackBlocks = 1000;
constexpr int kMaximumUploadAttempts = 5;
constexpr int kMaximumRetryDelayMs = 60000;

QString coordinate(double value, QChar positive, QChar negative)
{
    return QStringLiteral("%1%2")
        .arg(std::abs(value), 0, 'f', 6)
        .arg(value >= 0.0 ? positive : negative);
}

QPlainTextEdit* makeConsole(QWidget* parent)
{
    auto* console = new QPlainTextEdit(parent);
    console->setReadOnly(true);
    console->setLineWrapMode(QPlainTextEdit::NoWrap);
    console->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    console->setStyleSheet(QStringLiteral(
        "QPlainTextEdit { background:#090909; color:#e8e8e8; "
        "border:1px solid #a0a0a0; selection-background-color:#315c7d; }"));
    return console;
}

QGroupBox* wrapConsole(const QString& title, QPlainTextEdit* console,
                       QWidget* parent)
{
    auto* group = new QGroupBox(title, parent);
    auto* layout = new QVBoxLayout(group);
    layout->setContentsMargins(4, 8, 4, 4);
    layout->addWidget(console);
    return group;
}
} // namespace

bool validateSatnogsUploaderConfig(const SatnogsUploaderConfig& config,
                                   QString* error)
{
    const QUrl api(config.apiAddress);
    if (config.noradId <= 0) {
        if (error)
            *error = QCoreApplication::translate(
                "ASRTU", "SatNOGS NORAD ID 无效。");
        return false;
    }
    if (config.source.trimmed().isEmpty()) {
        if (error)
            *error = QCoreApplication::translate(
                "ASRTU", "地面站呼号或昵称不能为空。");
        return false;
    }
    if (config.longitude < -180.0 || config.longitude > 180.0 ||
        config.latitude < -90.0 || config.latitude > 90.0) {
        if (error)
            *error = QCoreApplication::translate(
                "ASRTU", "地面站经纬度超出有效范围。");
        return false;
    }
    if (!config.zmqAddress.startsWith(QStringLiteral("tcp://"))) {
        if (error)
            *error = QCoreApplication::translate(
                "ASRTU", "ZMQ 地址必须使用 tcp://。");
        return false;
    }
    if (!api.isValid() || api.scheme() != QStringLiteral("https") ||
        api.host().isEmpty()) {
        if (error)
            *error = QCoreApplication::translate(
                "ASRTU", "SatNOGS API 地址无效。");
        return false;
    }
    if (error)
        error->clear();
    return true;
}

QString formatTelemetryFrame(const QByteArray& frame)
{
    if (frame.isEmpty())
        return QCoreApplication::translate("ASRTU", "等待遥测帧…");
    return QCoreApplication::translate(
               "ASRTU", "已接收一帧遥测数据\n长度：%1 字节\n内容不在界面中展示。")
        .arg(frame.size());
}

bool isRetriableSatnogsUpload(int networkError, int httpStatus)
{
    if (networkError == int(QNetworkReply::NoError))
        return false;
    return httpStatus == 0 || httpStatus == 408 || httpStatus == 425 ||
           httpStatus == 429 || (httpStatus >= 500 && httpStatus <= 599);
}

int satnogsRetryDelayMs(int attempt, int retryAfterSeconds)
{
    const int boundedAttempt = std::clamp(attempt, 1, kMaximumUploadAttempts);
    const int backoff = 1000 * (1 << (boundedAttempt - 1));
    const int serverDelay = retryAfterSeconds < 0
                                ? 0
                                : std::min(retryAfterSeconds * 1000,
                                           kMaximumRetryDelayMs);
    return std::min(std::max(backoff, serverDelay), kMaximumRetryDelayMs);
}

SatnogsUploaderWindow::SatnogsUploaderWindow(SatnogsUploaderConfig config,
                                             QWidget* parent)
    : QMainWindow(parent), config_(std::move(config))
{
    receiveTimer_.setInterval(kReceiveIntervalMs);
    connect(&receiveTimer_, &QTimer::timeout, this,
            [this] { receiveFrames(); });
    retryTimer_.setSingleShot(true);
    connect(&retryTimer_, &QTimer::timeout, this,
            [this] { submitPending(); });
    buildUi();
}

SatnogsUploaderWindow::~SatnogsUploaderWindow()
{
    receiveTimer_.stop();
    if (zmqSocket_) {
        const int linger = 0;
        zmq_setsockopt(zmqSocket_, ZMQ_LINGER, &linger, sizeof(linger));
        zmq_close(zmqSocket_);
    }
    if (zmqContext_)
        zmq_ctx_term(zmqContext_);
}

void SatnogsUploaderWindow::buildUi()
{
    setWindowTitle(QCoreApplication::translate(
        "ASRTU", "ASRTU SatNOGS 遥测上传"));
    resize(1000, 560);
    setMinimumSize(760, 440);

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(5, 5, 5, 5);
    layout->setSpacing(4);

    auto* vertical = new QSplitter(Qt::Vertical, central);
    auto* upper = new QSplitter(Qt::Horizontal, vertical);
    frameView_ = makeConsole(upper);
    stationView_ = makeConsole(upper);
    feedbackView_ = makeConsole(vertical);
    feedbackView_->document()->setMaximumBlockCount(kMaximumFeedbackBlocks);

    upper->addWidget(wrapConsole(
        QCoreApplication::translate("ASRTU", "上传状态"),
        frameView_, upper));
    upper->addWidget(wrapConsole(
        QCoreApplication::translate("ASRTU", "站点与上传配置"),
        stationView_, upper));
    upper->setStretchFactor(0, 2);
    upper->setStretchFactor(1, 1);
    upper->setSizes({650, 330});

    vertical->addWidget(upper);
    vertical->addWidget(wrapConsole(
        QCoreApplication::translate("ASRTU", "服务器反馈"),
        feedbackView_, vertical));
    vertical->setStretchFactor(0, 3);
    vertical->setStretchFactor(1, 1);
    vertical->setSizes({380, 160});
    layout->addWidget(vertical);
    setCentralWidget(central);

    setStyleSheet(QStringLiteral(
        "QMainWindow,QWidget { background:#111111; color:#e8e8e8; }"
        "QGroupBox { border:0; margin-top:1.1em; font-weight:bold; }"
        "QGroupBox::title { subcontrol-origin:margin; left:5px; }"
        "QSplitter::handle { background:#606060; }"));
    frameView_->setPlainText(formatTelemetryFrame({}));
    stationView_->setPlainText(configurationText());
}

QString SatnogsUploaderWindow::configurationText() const
{
    return QCoreApplication::translate(
               "ASRTU",
               "Configuration:\n"
               "ZMQ Address:\n  %1\n"
               "SatNOGS API:\n  %2\n"
               "Satellite Name: %3\n"
               "NORAD ID: %4\n"
               "Ground Station: %5\n"
               "Longitude: %6\n"
               "Altitude: %7 m\n"
               "Latitude: %8")
        .arg(config_.zmqAddress, config_.apiAddress, config_.satellite,
             QString::number(config_.noradId), config_.source,
             QString::number(config_.longitude, 'f', 6),
             QString::number(config_.altitude, 'f', 2),
             QString::number(config_.latitude, 'f', 6));
}

bool SatnogsUploaderWindow::start(QString* error)
{
    if (!validateSatnogsUploaderConfig(config_, error))
        return false;

    zmqContext_ = zmq_ctx_new();
    if (!zmqContext_) {
        if (error)
            *error = QCoreApplication::translate(
                         "ASRTU", "无法创建 ZeroMQ 上下文：%1")
                         .arg(QString::fromLocal8Bit(zmq_strerror(errno)));
        return false;
    }
    zmqSocket_ = zmq_socket(zmqContext_, ZMQ_SUB);
    if (!zmqSocket_) {
        if (error)
            *error = QCoreApplication::translate(
                         "ASRTU", "无法创建 ZeroMQ 订阅套接字：%1")
                         .arg(QString::fromLocal8Bit(zmq_strerror(errno)));
        return false;
    }
    if (zmq_setsockopt(zmqSocket_, ZMQ_SUBSCRIBE, "", 0) != 0 ||
        zmq_connect(zmqSocket_, config_.zmqAddress.toUtf8().constData()) != 0) {
        if (error)
            *error = QCoreApplication::translate(
                         "ASRTU", "无法连接到 %1：%2")
                         .arg(config_.zmqAddress,
                              QString::fromLocal8Bit(zmq_strerror(errno)));
        return false;
    }

    network_ = new QNetworkAccessManager(this);
    receiveTimer_.start();
    appendFeedback(QCoreApplication::translate(
        "ASRTU", "已连接 ZeroMQ 订阅端，等待解码器帧。"));
    appendFeedback(QCoreApplication::translate(
        "ASRTU", "SatNOGS HTTPS 上传已就绪。"));
    if (error)
        error->clear();
    return true;
}

void SatnogsUploaderWindow::showPreviewFrame(const QByteArray& frame)
{
    frameView_->setPlainText(formatTelemetryFrame(frame));
    appendFeedback(QCoreApplication::translate(
        "ASRTU", "预览模式：已载入示例遥测帧。"));
}

void SatnogsUploaderWindow::receiveFrames()
{
    std::array<char, 65536> serialized{};
    for (int count = 0; count < kMaximumFramesPerTick; ++count) {
        const int received = zmq_recv(zmqSocket_, serialized.data(),
                                     serialized.size(), ZMQ_DONTWAIT);
        if (received < 0) {
            if (errno != EAGAIN) {
                appendFeedback(QCoreApplication::translate(
                                   "ASRTU", "ZeroMQ 接收失败：%1")
                                   .arg(QString::fromLocal8Bit(zmq_strerror(errno))));
            }
            return;
        }
        if (static_cast<std::size_t>(received) > serialized.size()) {
            appendFeedback(QCoreApplication::translate(
                               "ASRTU", "已丢弃超长 ZeroMQ 消息（%1 字节）。")
                               .arg(received));
            continue;
        }
        std::vector<std::uint8_t> decoded;
        std::string decodeError;
        if (!decodePmtTelemetryFrame(serialized.data(), received,
                                     &decoded, &decodeError)) {
            appendFeedback(QCoreApplication::translate(
                               "ASRTU", "已丢弃无效遥测 PDU：%1")
                               .arg(QString::fromStdString(decodeError)));
            continue;
        }
        enqueueFrame(QByteArray(
            reinterpret_cast<const char*>(decoded.data()),
            static_cast<int>(decoded.size())));
    }
}

void SatnogsUploaderWindow::enqueueFrame(const QByteArray& frame)
{
    ++receivedFrames_;
    frameView_->setPlainText(
        QCoreApplication::translate("ASRTU", "接收序号：%1\n接收时间：%2 UTC\n\n")
            .arg(receivedFrames_)
            .arg(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)) +
        formatTelemetryFrame(frame));
    frameView_->verticalScrollBar()->setValue(0);

    if (pendingFrames_.size() >= kMaximumPendingFrames) {
        const bool containsRetry = std::any_of(
            pendingFrames_.cbegin(), pendingFrames_.cend(),
            [](const PendingFrame& pending) { return pending.attempts > 0; });
        if (containsRetry) {
            appendFeedback(QCoreApplication::translate(
                "ASRTU", "上传队列已满且包含待重试帧，已丢弃新收到的一帧。"));
            return;
        }
        pendingFrames_.dequeue();
        appendFeedback(QCoreApplication::translate(
            "ASRTU", "上传队列已满，已丢弃最旧的一帧。"));
    }
    pendingFrames_.enqueue(
        {frame, QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)});
    appendFeedback(QCoreApplication::translate(
                       "ASRTU", "收到遥测帧 #%1（%2 字节），加入上传队列。")
                       .arg(receivedFrames_)
                       .arg(frame.size()));
    submitPending();
}

void SatnogsUploaderWindow::submitPending()
{
    if (retryTimer_.isActive())
        return;
    while (inFlight_ < kMaximumInFlightRequests && !pendingFrames_.isEmpty())
        submitFrame(pendingFrames_.dequeue());
}

void SatnogsUploaderWindow::submitFrame(PendingFrame frame)
{
    ++frame.attempts;
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("noradID"),
                      QString::number(config_.noradId));
    form.addQueryItem(QStringLiteral("source"), config_.source);
    form.addQueryItem(QStringLiteral("locator"), QStringLiteral("longLat"));
    form.addQueryItem(QStringLiteral("longitude"),
                      coordinate(config_.longitude, QLatin1Char('E'),
                                 QLatin1Char('W')));
    form.addQueryItem(QStringLiteral("latitude"),
                      coordinate(config_.latitude, QLatin1Char('N'),
                                 QLatin1Char('S')));
    form.addQueryItem(QStringLiteral("version"), QStringLiteral("1.6.6"));
    form.addQueryItem(QStringLiteral("frame"),
                      QString::fromLatin1(frame.bytes.toHex().toUpper()));
    form.addQueryItem(QStringLiteral("timestamp"), frame.timestamp);

    QNetworkRequest request{QUrl(config_.apiAddress)};
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
    request.setRawHeader("User-Agent", "ASRTU-Series-Receiver/1.5.4");
    ++inFlight_;
    QNetworkReply* reply = network_->post(
        request, form.query(QUrl::FullyEncoded).toUtf8());
    const QPointer<QNetworkReply> guardedReply(reply);
    QTimer::singleShot(kRequestTimeoutMs, reply, [guardedReply] {
        if (guardedReply && !guardedReply->isFinished()) {
            guardedReply->setProperty("asrtuTimedOut", true);
            guardedReply->abort();
        }
    });
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, frame] {
        --inFlight_;
        const int status = reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QString response = QString::fromUtf8(reply->readAll()).trimmed();
        if (response.size() > kMaximumResponseCharacters)
            response = response.left(kMaximumResponseCharacters) +
                       QStringLiteral("…");
        const int networkError = int(reply->error());
        const bool retry = frame.attempts < kMaximumUploadAttempts &&
                           isRetriableSatnogsUpload(networkError, status);
        if (reply->error() == QNetworkReply::NoError) {
            appendFeedback(QCoreApplication::translate(
                               "ASRTU", "SatNOGS 接受 %1 字节帧（HTTP %2）。")
                               .arg(frame.bytes.size())
                               .arg(status));
        } else if (reply->property("asrtuTimedOut").toBool()) {
            appendFeedback(QCoreApplication::translate(
                               "ASRTU", "SatNOGS 上传超时（15 秒），请求已取消。"));
        } else {
            appendFeedback(QCoreApplication::translate(
                               "ASRTU", "SatNOGS 上传失败（HTTP %1）：%2")
                               .arg(status)
                               .arg(reply->errorString()));
        }
        if (!response.isEmpty()) {
            appendFeedback(QCoreApplication::translate(
                               "ASRTU", "服务器响应：%1")
                               .arg(response));
        }
        bool retryAfterOk = false;
        const int retryAfterSeconds =
            reply->rawHeader("Retry-After").toInt(&retryAfterOk);
        reply->deleteLater();
        if (retry) {
            if (pendingFrames_.size() >= kMaximumPendingFrames) {
                pendingFrames_.removeLast();
                appendFeedback(QCoreApplication::translate(
                    "ASRTU", "重试队列已满，已丢弃最新的一帧。"));
            }
            pendingFrames_.prepend(frame);
            const int delay = satnogsRetryDelayMs(
                frame.attempts, retryAfterOk ? retryAfterSeconds : -1);
            retryTimer_.start(std::max(retryTimer_.remainingTime(), delay));
            appendFeedback(QCoreApplication::translate(
                "ASRTU", "将在 %1 秒后重试该帧（第 %2/%3 次）。")
                .arg(delay / 1000.0, 0, 'f', 1)
                .arg(frame.attempts + 1)
                .arg(kMaximumUploadAttempts));
        } else if (!retryTimer_.isActive()) {
            submitPending();
        }
    });
}

void SatnogsUploaderWindow::appendFeedback(const QString& message)
{
    feedbackView_->appendPlainText(
        QStringLiteral("[%1] %2")
            .arg(QDateTime::currentDateTime().toString(
                     QStringLiteral("yyyy-MM-dd HH:mm:ss")),
                 message));
    feedbackView_->verticalScrollBar()->setValue(
        feedbackView_->verticalScrollBar()->maximum());
}

} // namespace asrtu
