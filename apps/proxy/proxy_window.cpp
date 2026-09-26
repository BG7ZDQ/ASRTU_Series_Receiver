#include "proxy_window.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFontDatabase>
#include <QGroupBox>
#include <QIcon>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QSplitter>
#include <QTextDocument>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

namespace asrtu
{
namespace
{
constexpr int kRefreshIntervalMs = 100;
constexpr int kMaximumFeedbackBlocks = 1000;
constexpr int kMaximumServerMessageCharacters = 2048;

QPlainTextEdit *makeConsole(QWidget *parent)
{
	auto *console = new QPlainTextEdit(parent);
	console->setReadOnly(true);
	console->setLineWrapMode(QPlainTextEdit::NoWrap);
	console->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
	console->setStyleSheet(QStringLiteral(
	    "QPlainTextEdit { background:#090909; color:#e8e8e8; "
	    "border:1px solid #a0a0a0; selection-background-color:#315c7d; }"));
	return console;
}

QGroupBox *wrapConsole(const QString &title, QPlainTextEdit *console,
		       QWidget *parent)
{
	auto *group = new QGroupBox(title, parent);
	auto *layout = new QVBoxLayout(group);
	layout->setContentsMargins(4, 8, 4, 4);
	layout->addWidget(console);
	return group;
}

QString formatTelemetryFrame(const QByteArray &frame)
{
	if (frame.isEmpty())
		return QCoreApplication::translate("ASRTU", "等待遥测帧…");
	QString dump = QCoreApplication::translate(
			   "ASRTU", "已接收一帧遥测数据\n长度：%1 字节")
			   .arg(frame.size());
	constexpr int kBytesPerLine = 16;
	for (int offset = 0; offset < frame.size();
	     offset += kBytesPerLine) {
		const QByteArray hex =
		    frame.mid(offset, kBytesPerLine).toHex(' ').toUpper();
		dump += QStringLiteral("\n%1: %2")
			    .arg(static_cast<qulonglong>(offset), 4, 16,
				 QLatin1Char('0'))
			    .arg(QString::fromLatin1(hex))
			    .toUpper();
	}
	return dump;
}

QString connectionText(QAbstractSocket::SocketState state)
{
	switch (state) {
	case QAbstractSocket::ConnectedState:
		return QCoreApplication::translate("ProxyWindow", "已连接");
	case QAbstractSocket::HostLookupState:
	case QAbstractSocket::ConnectingState:
		return QCoreApplication::translate("ProxyWindow", "连接中");
	default:
		return QCoreApplication::translate("ProxyWindow", "等待重连");
	}
}
} // namespace

ProxyWindow::ProxyWindow(const ProxyConfig &config, UploadProxy &proxy,
			 QWidget *parent)
    : QMainWindow(parent), config_(config), proxy_(proxy)
{
	buildUi();
	proxy_.setFeedbackHandler(
	    [this](ProxyEvent event, const QString &detail) {
		    handleProxyEvent(event, detail);
	    });
	appendFeedback(QCoreApplication::translate(
	    "ProxyWindow", "已连接 ZeroMQ 订阅端，等待解码器帧。"));
	refreshStatus();
	refreshTimer_.setInterval(kRefreshIntervalMs);
	connect(&refreshTimer_, &QTimer::timeout, this,
		&ProxyWindow::refreshStatus);
	refreshTimer_.start();
}

ProxyWindow::~ProxyWindow()
{
	refreshTimer_.stop();
	proxy_.setFeedbackHandler({});
}

void ProxyWindow::buildUi()
{
	setWindowTitle(QCoreApplication::translate(
	    "ProxyWindow", "ASRTU MMT 遥测上传"));
	setWindowIcon(QIcon(QStringLiteral(":/proxy/icon.png")));
	resize(1000, 560);
	setMinimumSize(760, 440);

	auto *central = new QWidget(this);
	auto *layout = new QVBoxLayout(central);
	layout->setContentsMargins(5, 5, 5, 5);
	layout->setSpacing(4);

	auto *vertical = new QSplitter(Qt::Vertical, central);
	auto *upper = new QSplitter(Qt::Horizontal, vertical);
	frameView_ = makeConsole(upper);
	stationView_ = makeConsole(upper);
	feedbackView_ = makeConsole(vertical);
	feedbackView_->document()->setMaximumBlockCount(kMaximumFeedbackBlocks);

	upper->addWidget(wrapConsole(
	    QCoreApplication::translate("ASRTU", "上传状态"), frameView_, upper));
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

QString ProxyWindow::configurationText() const
{
	QUrl server(config_.webSocketAddress);
	server.setPort(config_.webSocketPort);
	return QCoreApplication::translate(
		   "ProxyWindow",
		   "配置：\n"
		   "ZMQ 地址：\n  %1\n"
		   "WebSocket 地址：\n  %2\n"
		   "卫星：%3\n"
		   "物理信道：%4\n"
		   "呼号：%5\n"
		   "经度：%6\n"
		   "海拔：%7 m\n"
		   "纬度：%8")
	    .arg(config_.zmqAddress, server.toString(), config_.satellite,
		 QString::number(config_.physicalChannel), config_.nickname,
		 QString::number(config_.longitude, 'f', 6),
		 QString::number(config_.altitude, 'f', 2),
		 QString::number(config_.latitude, 'f', 6));
}

void ProxyWindow::refreshStatus()
{
	const ProxySnapshot current = proxy_.snapshot();
	if (hasRendered_ && current.connectionState == displayed_.connectionState &&
	    current.receivedFrames == displayed_.receivedFrames &&
	    current.sentFrames == displayed_.sentFrames &&
	    current.pendingFrames == displayed_.pendingFrames &&
	    current.droppedFrames == displayed_.droppedFrames &&
	    current.invalidFrames == displayed_.invalidFrames)
		return;

	if (current.receivedFrames > displayed_.receivedFrames) {
		appendFeedback(QCoreApplication::translate(
				   "ProxyWindow",
				   "收到 %1 帧遥测数据；累计接收 %2，已提交 %3，待发送 %4。")
				   .arg(current.receivedFrames -
					displayed_.receivedFrames)
				   .arg(current.receivedFrames)
				   .arg(current.sentFrames)
				   .arg(current.pendingFrames));
	}
	if (current.invalidFrames > displayed_.invalidFrames) {
		appendFeedback(QCoreApplication::translate(
				   "ProxyWindow", "已丢弃 %1 个无效遥测 PDU。")
				   .arg(current.invalidFrames -
					displayed_.invalidFrames));
	}
	if (current.droppedFrames > displayed_.droppedFrames) {
		appendFeedback(QCoreApplication::translate(
				   "ProxyWindow",
				   "待发送队列已满，丢弃 %1 帧。")
				   .arg(current.droppedFrames -
					displayed_.droppedFrames));
	}
	if (hasRendered_ && current.sentFrames > displayed_.sentFrames &&
	    current.receivedFrames == displayed_.receivedFrames) {
		appendFeedback(QCoreApplication::translate(
				   "ProxyWindow",
				   "已向 WebSocket 提交 %1 帧缓存数据。")
				   .arg(current.sentFrames - displayed_.sentFrames));
	}

	QString text = QCoreApplication::translate(
			   "ProxyWindow",
			   "WebSocket：%1\n"
			   "已接收：%2  已提交：%3  待发送：%4\n"
			   "队列丢弃：%5  无效 PDU：%6\n\n")
			   .arg(connectionText(current.connectionState))
			   .arg(current.receivedFrames)
			   .arg(current.sentFrames)
			   .arg(current.pendingFrames)
			   .arg(current.droppedFrames)
			   .arg(current.invalidFrames);
	if (!current.lastFrame.isEmpty()) {
		text += QCoreApplication::translate(
			    "ASRTU", "接收序号：%1\n接收时间：%2 UTC\n\n")
			    .arg(current.receivedFrames)
			    .arg(current.lastFrameTimeUtc.toString(
				Qt::ISODateWithMs));
	}
	text += formatTelemetryFrame(current.lastFrame);
	frameView_->setPlainText(text);
	frameView_->verticalScrollBar()->setValue(0);
	displayed_ = current;
	hasRendered_ = true;
}

void ProxyWindow::handleProxyEvent(ProxyEvent event, const QString &detail)
{
	switch (event) {
	case ProxyEvent::Connected:
		appendFeedback(QCoreApplication::translate(
		    "ProxyWindow", "WebSocket 已连接。"));
		break;
	case ProxyEvent::Disconnected:
		appendFeedback(QCoreApplication::translate(
		    "ProxyWindow", "WebSocket 已断开，正在重连。"));
		break;
	case ProxyEvent::ConnectionError:
		appendFeedback(QCoreApplication::translate(
				   "ProxyWindow", "WebSocket 错误：%1")
				   .arg(detail));
		break;
	case ProxyEvent::ServerMessage:
		appendFeedback(QCoreApplication::translate(
				   "ProxyWindow", "服务器响应：%1")
				   .arg(detail));
		break;
	}
}

void ProxyWindow::appendFeedback(const QString &message)
{
	QString oneLine = message.left(kMaximumServerMessageCharacters);
	oneLine.replace(QLatin1Char('\r'), QLatin1Char(' '));
	oneLine.replace(QLatin1Char('\n'), QLatin1Char(' '));
	feedbackView_->appendPlainText(
	    QStringLiteral("[%1] %2")
		.arg(QDateTime::currentDateTime().toString(
			 QStringLiteral("yyyy-MM-dd HH:mm:ss")),
		     oneLine));
	feedbackView_->verticalScrollBar()->setValue(
	    feedbackView_->verticalScrollBar()->maximum());
}
} // namespace asrtu
