#include "proxy_config.h"
#include "runtime_paths.h"
#include "upload_proxy.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QString>

#include <utility>

int main(int argc, char *argv[])
{
	QCoreApplication application(argc, argv);
	QCoreApplication::setApplicationName(
	    QStringLiteral("ASRTU Upload Proxy"));

	QString configPath = asrtu::proxyConfigPath();
	if (application.arguments().size() == 3 &&
	    application.arguments().at(1) == QStringLiteral("--config")) {
		configPath = application.arguments().at(2);
	} else if (application.arguments().size() != 1) {
		qCritical("usage: %s [--config PATH]", argv[0]);
		return 2;
	}

	asrtu::ProxyConfig config;
	QString error;
	if (!asrtu::loadProxyConfig(configPath, &config, &error)) {
		qCritical("%s", qPrintable(error));
		return 1;
	}
	qInfo("Using proxy configuration %s",
	      qPrintable(QFileInfo(configPath).absoluteFilePath()));

	asrtu::UploadProxy proxy(std::move(config));
	if (!proxy.start(&error)) {
		qCritical("%s", qPrintable(error));
		return 1;
	}
	return application.exec();
}
