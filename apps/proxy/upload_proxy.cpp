#include "upload_proxy.h"

#include "pmt_frame_decoder.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QtGlobal>
#include <zmq.h>

#include <array>
#include <cerrno>
#include <string>
#include <utility>
#include <vector>

namespace asrtu
{
namespace
{
constexpr int kReceiveIntervalMs = 10;
constexpr int kReconnectIntervalMs = 3000;
constexpr int kMaximumPendingFrames = 64;
constexpr int kMaximumFramesPerTick = 64;
} // namespace

UploadProxy::UploadProxy(ProxyConfig config) : config_(std::move(config))
{
	receiveTimer_.setInterval(kReceiveIntervalMs);
	reconnectTimer_.setInterval(kReconnectIntervalMs);
	reconnectTimer_.setSingleShot(true);

	QObject::connect(&receiveTimer_, &QTimer::timeout,
			 [&] { receiveFrames(); });
	QObject::connect(&reconnectTimer_, &QTimer::timeout,
			 [&] { connectWebSocket(); });
	QObject::connect(&webSocket_, &QWebSocket::connected, [&] {
		qInfo("WebSocket connection established");
		flushPending();
	});
	QObject::connect(&webSocket_, &QWebSocket::disconnected, [&] {
		qWarning("WebSocket connection closed; reconnecting");
		reconnectTimer_.start();
	});
	QObject::connect(
	    &webSocket_,
	    QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
	    [&](QAbstractSocket::SocketError) {
		    qWarning("WebSocket error: %s",
			     qPrintable(webSocket_.errorString()));
		    if (webSocket_.state() == QAbstractSocket::UnconnectedState)
			    reconnectTimer_.start();
	    });
}

UploadProxy::~UploadProxy()
{
	receiveTimer_.stop();
	reconnectTimer_.stop();
	webSocket_.close();
	if (zmqSocket_)
		zmq_close(zmqSocket_);
	if (zmqContext_)
		zmq_ctx_term(zmqContext_);
}

bool UploadProxy::start(QString *error)
{
	zmqContext_ = zmq_ctx_new();
	if (!zmqContext_) {
		if (error)
			*error =
			    QStringLiteral("cannot create ZeroMQ context: %1")
				.arg(QString::fromLocal8Bit(
				    zmq_strerror(errno)));
		return false;
	}
	zmqSocket_ = zmq_socket(zmqContext_, ZMQ_SUB);
	if (!zmqSocket_) {
		if (error)
			*error =
			    QStringLiteral("cannot create ZeroMQ socket: %1")
				.arg(QString::fromLocal8Bit(
				    zmq_strerror(errno)));
		return false;
	}
	if (zmq_setsockopt(zmqSocket_, ZMQ_SUBSCRIBE, "", 0) != 0 ||
	    zmq_connect(zmqSocket_, config_.zmqAddress.toUtf8().constData()) !=
		0) {
		if (error)
			*error = QStringLiteral("cannot connect to %1: %2")
				     .arg(config_.zmqAddress,
					  QString::fromLocal8Bit(
					      zmq_strerror(errno)));
		return false;
	}

	qInfo("ZeroMQ subscriber connected to %s",
	      qPrintable(config_.zmqAddress));
	receiveTimer_.start();
	connectWebSocket();
	return true;
}

void UploadProxy::connectWebSocket()
{
	if (webSocket_.state() != QAbstractSocket::UnconnectedState)
		return;
	QUrl url(config_.webSocketAddress);
	url.setPort(config_.webSocketPort);
	if (url.path().isEmpty())
		url.setPath(QStringLiteral("/"));
	qInfo("Connecting WebSocket to %s", qPrintable(url.toString()));
	webSocket_.open(url);
}

void UploadProxy::receiveFrames()
{
	std::array<char, 65536> serialized{};
	for (int count = 0; count < kMaximumFramesPerTick; ++count) {
		const int received = zmq_recv(zmqSocket_, serialized.data(),
					      serialized.size(), ZMQ_DONTWAIT);
		if (received < 0) {
			if (errno != EAGAIN)
				qWarning("ZeroMQ receive failed: %s",
					 zmq_strerror(errno));
			return;
		}
		if (static_cast<std::size_t>(received) > serialized.size()) {
			qWarning(
			    "Discarded oversized ZeroMQ message (%d bytes)",
			    received);
			continue;
		}
		std::vector<std::uint8_t> frame;
		std::string decodeError;
		if (!decodePmtTelemetryFrame(serialized.data(), received,
					     &frame, &decodeError)) {
			qWarning("Discarded invalid telemetry PDU: %s",
				 decodeError.c_str());
			continue;
		}
		submitFrame(
		    QByteArray(reinterpret_cast<const char *>(frame.data()),
			       static_cast<int>(frame.size())));
	}
}

void UploadProxy::submitFrame(const QByteArray &frame)
{
	QJsonObject object{
	    {QStringLiteral("sat_name"), config_.satellite},
	    {QStringLiteral("physical_channel"), config_.physicalChannel},
	    {QStringLiteral("proxy_nickname"), config_.nickname},
	    {QStringLiteral("proxy_long"), config_.longitude},
	    {QStringLiteral("proxy_alt"), config_.altitude},
	    {QStringLiteral("proxy_lat"), config_.latitude},
	    {QStringLiteral("raw_data"),
	     QString::fromLatin1(frame.toHex().toUpper())},
	    {QStringLiteral("proxy_receive_time"),
	     static_cast<double>(QDateTime::currentMSecsSinceEpoch())},
	};
	const QByteArray message =
	    QJsonDocument(object).toJson(QJsonDocument::Compact);
	if (webSocket_.state() == QAbstractSocket::ConnectedState) {
		webSocket_.sendTextMessage(QString::fromUtf8(message));
		return;
	}
	if (pendingFrames_.size() == kMaximumPendingFrames)
		pendingFrames_.dequeue();
	pendingFrames_.enqueue(message);
}

void UploadProxy::flushPending()
{
	while (!pendingFrames_.isEmpty() &&
	       webSocket_.state() == QAbstractSocket::ConnectedState) {
		webSocket_.sendTextMessage(
		    QString::fromUtf8(pendingFrames_.dequeue()));
	}
}

} // namespace asrtu
