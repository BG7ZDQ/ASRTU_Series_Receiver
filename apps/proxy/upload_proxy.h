#pragma once

#include "proxy_config.h"

#include <QQueue>
#include <QTimer>
#include <QWebSocket>

namespace asrtu
{

class UploadProxy final
{
      public:
	explicit UploadProxy(ProxyConfig config);
	~UploadProxy();

	bool start(QString *error);

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
};

} // namespace asrtu
