#pragma once

#include "proxy_config.h"

#include <QAbstractSocket>
#include <QDateTime>
#include <QQueue>
#include <QTimer>
#include <QWebSocket>

#include <cstdint>
#include <functional>

namespace asrtu
{
enum class ProxyEvent {
	Connected,
	Disconnected,
	ConnectionError,
	ServerMessage,
};

struct ProxySnapshot {
	QAbstractSocket::SocketState connectionState =
	    QAbstractSocket::UnconnectedState;
	std::uint64_t receivedFrames = 0;
	std::uint64_t sentFrames = 0;
	std::uint64_t invalidFrames = 0;
	std::uint64_t droppedFrames = 0;
	int pendingFrames = 0;
	QByteArray lastFrame;
	QDateTime lastFrameTimeUtc;
};

class UploadProxy final
{
      public:
	using FeedbackHandler =
	    std::function<void(ProxyEvent, const QString &)>;

	explicit UploadProxy(ProxyConfig config);
	~UploadProxy();

	bool start(QString *error);
	ProxySnapshot snapshot() const;
	void setFeedbackHandler(FeedbackHandler handler);

      private:
	void connectWebSocket();
	void receiveFrames();
	void submitFrame(const QByteArray &frame);
	void flushPending();

	ProxyConfig config_;
	void *zmqContext_ = nullptr;
	void *zmqSocket_ = nullptr;
	QWebSocket webSocket_;
	QTimer receiveTimer_;
	QTimer reconnectTimer_;
	QQueue<QByteArray> pendingFrames_;
	ProxySnapshot snapshot_;
	FeedbackHandler feedbackHandler_;
};

} // namespace asrtu
