#pragma once

#include <QString>

namespace asrtu
{

struct ProxyConfig {
	QString zmqAddress;
	QString webSocketAddress;
	int webSocketPort = 0;
	QString satellite;
	int physicalChannel = 0;
	QString nickname;
	double longitude = 0.0;
	double latitude = 0.0;
	double altitude = 0.0;
};

bool loadProxyConfig(const QString &path, ProxyConfig *config, QString *error);

} // namespace asrtu
