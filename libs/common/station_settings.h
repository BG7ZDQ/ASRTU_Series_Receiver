#pragma once

#include <QFile>
#include <QRegularExpression>
#include <QSettings>

namespace asrtu
{

// Import only the station fields owned by the launcher. The legacy proxy's
// libconfig file remains a generated adapter for that external executable.
inline bool importProxySettings(const QString &path, QSettings &settings)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		return false;
	const QString text = QString::fromUtf8(file.readAll());
	for (const auto &mapping : {qMakePair(QStringLiteral("proxy_nickname"),
					      QStringLiteral("nickname")),
				    qMakePair(QStringLiteral("sat_name"),
					      QStringLiteral("satellite"))}) {
		const QRegularExpression expression(
		    QStringLiteral("(?:^|\\n)\\s*%1\\s*=\\s*\"([^\"]*)\"\\s*;")
			.arg(mapping.first));
		const auto match = expression.match(text);
		if (match.hasMatch())
			settings.setValue(mapping.second, match.captured(1));
	}
	for (const auto &mapping : {qMakePair(QStringLiteral("proxy_long"),
					      QStringLiteral("longitude")),
				    qMakePair(QStringLiteral("proxy_lat"),
					      QStringLiteral("latitude")),
				    qMakePair(QStringLiteral("proxy_alt"),
					      QStringLiteral("altitude"))}) {
		const QRegularExpression expression(
		    QStringLiteral("(?:^|\\n)\\s*%1\\s*=\\s*([-+0-9.eE]+)\\s*;")
			.arg(mapping.first));
		bool ok = false;
		const double value =
		    expression.match(text).captured(1).toDouble(&ok);
		if (ok)
			settings.setValue(mapping.second, value);
	}
	settings.sync();
	return settings.status() == QSettings::NoError;
}

} // namespace asrtu
