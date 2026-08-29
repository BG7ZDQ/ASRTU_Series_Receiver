#include "proxy_config.h"

#include <QCoreApplication>
#include <QTemporaryFile>

#include <iostream>

int main(int argc, char *argv[])
{
	QCoreApplication application(argc, argv);
	QTemporaryFile file;
	if (!file.open()) {
		std::cerr << "Could not create proxy configuration fixture\n";
		return 1;
	}
	const QByteArray contents = "# test fixture\n"
				    "zmq_address = \"tcp://127.0.0.1:5555\";\n"
				    "ws_address = \"ws://127.0.0.1\";\n"
				    "ws_port = 9000;\n"
				    "sat_name = \"ASRTU-1\";\n"
				    "physical_channel = 2;\n"
				    "proxy_nickname = \"N0CALL\";\n"
				    "proxy_long = 120.123456;\n"
				    "proxy_lat = -30.654321;\n"
				    "proxy_alt = 12.5;\n";
	if (file.write(contents) != contents.size() || !file.flush()) {
		std::cerr << "Could not write proxy configuration fixture\n";
		return 1;
	}

	asrtu::ProxyConfig config;
	QString error;
	if (!asrtu::loadProxyConfig(file.fileName(), &config, &error)) {
		std::cerr << "Valid proxy configuration was rejected: "
			  << error.toStdString() << '\n';
		return 1;
	}
	if (config.zmqAddress != QStringLiteral("tcp://127.0.0.1:5555") ||
	    config.webSocketPort != 9000 ||
	    config.satellite != QStringLiteral("ASRTU-1") ||
	    config.physicalChannel != 2 ||
	    config.nickname != QStringLiteral("N0CALL") ||
	    config.longitude != 120.123456 || config.latitude != -30.654321 ||
	    config.altitude != 12.5) {
		std::cerr
		    << "Proxy configuration values were parsed incorrectly\n";
		return 1;
	}

	QTemporaryFile incomplete;
	if (!incomplete.open() || incomplete.write("ws_port = 9000;\n") < 0 ||
	    !incomplete.flush()) {
		std::cerr
		    << "Could not write incomplete configuration fixture\n";
		return 1;
	}
	if (asrtu::loadProxyConfig(incomplete.fileName(), &config, &error)) {
		std::cerr << "Incomplete proxy configuration was accepted\n";
		return 1;
	}
	return 0;
}
