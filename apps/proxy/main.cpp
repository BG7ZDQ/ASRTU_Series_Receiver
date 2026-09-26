#include "proxy_config.h"
#include "proxy_window.h"
#include "runtime_paths.h"
#include "translation.h"
#include "upload_proxy.h"

#include <QApplication>
#include <QCoreApplication>
#include <QFileInfo>
#include <QGuiApplication>
#include <QString>
#include <QStringList>
#include <QTranslator>

namespace
{
int runProxy(QCoreApplication &application, bool graphical)
{
	QString configPath = asrtu::proxyConfigPath();
	const QStringList arguments = application.arguments();
	for (int index = 1; index < arguments.size(); ++index) {
		const QString &argument = arguments.at(index);
		if (argument == QStringLiteral("--config") &&
		    index + 1 < arguments.size()) {
			configPath = arguments.at(++index);
		} else if (argument == QStringLiteral("--gui") ||
			   argument.startsWith(QStringLiteral("--language="))) {
			continue;
		} else {
			qCritical("usage: %s [--config PATH] [--gui] [--language=LANG]",
				  application.applicationFilePath().toLocal8Bit().constData());
			return 2;
		}
	}

	asrtu::ProxyConfig config;
	QString error;
	if (!asrtu::loadProxyConfig(configPath, &config, &error)) {
		qCritical("%s", qPrintable(error));
		return 1;
	}
	qInfo("Using proxy configuration %s",
	      qPrintable(QFileInfo(configPath).absoluteFilePath()));

	asrtu::UploadProxy proxy(config);
	if (!proxy.start(&error)) {
		qCritical("%s", qPrintable(error));
		return 1;
	}
	if (graphical) {
		asrtu::ProxyWindow window(config, proxy);
		window.show();
		return application.exec();
	}
	return application.exec();
}
} // namespace

int main(int argc, char *argv[])
{
	QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
	QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
	QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
	    Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif
	bool graphical = false;
	for (int index = 1; index < argc; ++index) {
		if (QString::fromLocal8Bit(argv[index]) == QStringLiteral("--gui")) {
			graphical = true;
			break;
		}
	}
	if (graphical) {
		QApplication application(argc, argv);
		QCoreApplication::setApplicationName(
		    QStringLiteral("ASRTU Upload Proxy"));
		QTranslator translator;
		installSystemTranslation(application, translator);
		return runProxy(application, true);
	}
	QCoreApplication application(argc, argv);
	QCoreApplication::setApplicationName(QStringLiteral("ASRTU Upload Proxy"));
	return runProxy(application, false);
}
