#pragma once

#include "proxy_config.h"
#include "upload_proxy.h"

#include <QMainWindow>
#include <QTimer>

class QPlainTextEdit;

namespace asrtu
{
class ProxyWindow final : public QMainWindow
{
      public:
	explicit ProxyWindow(const ProxyConfig &config, UploadProxy &proxy,
			     QWidget *parent = nullptr);
	~ProxyWindow() override;

      private:
	void buildUi();
	void refreshStatus();
	void appendFeedback(const QString &message);
	void handleProxyEvent(ProxyEvent event, const QString &detail);
	QString configurationText() const;

	ProxyConfig config_;
	UploadProxy &proxy_;
	QPlainTextEdit *frameView_ = nullptr;
	QPlainTextEdit *stationView_ = nullptr;
	QPlainTextEdit *feedbackView_ = nullptr;
	QTimer refreshTimer_;
	ProxySnapshot displayed_;
	bool hasRendered_ = false;
};
} // namespace asrtu
