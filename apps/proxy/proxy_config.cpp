#include "proxy_config.h"

#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QUrl>

namespace asrtu
{

bool loadProxyConfig(const QString &path, ProxyConfig *config, QString *error)
{
	if (!config) {
		if (error)
			*error = QStringLiteral("missing configuration output");
		return false;
	}
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		if (error)
			*error = QStringLiteral("cannot read %1: %2")
				     .arg(path, file.errorString());
		return false;
	}

	QHash<QString, QString> values;
	const QRegularExpression assignment(QStringLiteral(
	    R"regex(^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(?:"((?:\\.|[^"])*)"|([^;]+))\s*;\s*$)regex"));
	while (!file.atEnd()) {
		const QString line = QString::fromUtf8(file.readLine());
		const auto match = assignment.match(line);
		if (!match.hasMatch())
			continue;
		QString value = match.captured(2).isNull()
				    ? match.captured(3).trimmed()
				    : match.captured(2);
		value.replace(QStringLiteral("\\\""), QStringLiteral("\""));
		value.replace(QStringLiteral("\\\\"), QStringLiteral("\\"));
		values.insert(match.captured(1), value);
	}

	auto require = [&](const QString &key) {
		if (values.contains(key) && !values.value(key).isEmpty())
			return true;
		if (error)
			*error = QStringLiteral("missing configuration key: %1")
				     .arg(key);
		return false;
	};
	for (const QString &key :
	     {QStringLiteral("zmq_address"), QStringLiteral("ws_address"),
	      QStringLiteral("ws_port"), QStringLiteral("sat_name"),
	      QStringLiteral("proxy_nickname"), QStringLiteral("proxy_long"),
	      QStringLiteral("proxy_lat"), QStringLiteral("proxy_alt")}) {
		if (!require(key))
			return false;
	}

	bool portOk = false;
	bool channelOk = true;
	bool longitudeOk = false;
	bool latitudeOk = false;
	bool altitudeOk = false;
	config->zmqAddress = values.value(QStringLiteral("zmq_address"));
	config->webSocketAddress = values.value(QStringLiteral("ws_address"));
	config->webSocketPort =
	    values.value(QStringLiteral("ws_port")).toInt(&portOk);
	config->satellite = values.value(QStringLiteral("sat_name"));
	config->physicalChannel =
	    values
		.value(QStringLiteral("physical_channel"), QStringLiteral("0"))
		.toInt(&channelOk);
	config->nickname = values.value(QStringLiteral("proxy_nickname"));
	config->longitude =
	    values.value(QStringLiteral("proxy_long")).toDouble(&longitudeOk);
	config->latitude =
	    values.value(QStringLiteral("proxy_lat")).toDouble(&latitudeOk);
	config->altitude =
	    values.value(QStringLiteral("proxy_alt")).toDouble(&altitudeOk);
	if (!portOk || config->webSocketPort < 1 ||
	    config->webSocketPort > 65535 || !channelOk || !longitudeOk ||
	    !latitudeOk || !altitudeOk) {
		if (error)
			*error = QStringLiteral("invalid numeric value in %1")
				     .arg(path);
		return false;
	}
	const QUrl webSocketUrl(config->webSocketAddress);
	if (!webSocketUrl.isValid() || webSocketUrl.host().isEmpty() ||
	    (webSocketUrl.scheme() != QStringLiteral("ws") &&
	     webSocketUrl.scheme() != QStringLiteral("wss"))) {
		if (error)
			*error =
			    QStringLiteral("invalid WebSocket address in %1")
				.arg(path);
		return false;
	}
	if (error)
		error->clear();
	return true;
}

} // namespace asrtu
