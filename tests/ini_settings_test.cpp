#include "ini_settings.h"
#include "station_settings.h"
#include <QFile>
#include <QTemporaryDir>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

int main(int argc, char **argv)
{
	QCoreApplication app(argc, argv);
	QTemporaryDir temporary;
	assert(temporary.isValid());
	qunsetenv("ASRTU_CONFIG_DIR");
#ifdef Q_OS_WIN
	const QString appRoot =
	    temporary.filePath(QStringLiteral("Portable suite"));
	QDir().mkpath(appRoot + QStringLiteral("/bin"));
	assert(asrtu::settingsDirectory(appRoot + QStringLiteral("/bin")) ==
	       appRoot + QStringLiteral("/config"));
#else
	assert(asrtu::settingsDirectory() ==
	       QDir(QStandardPaths::writableLocation(
			QStandardPaths::GenericConfigLocation))
		   .filePath(QStringLiteral("ASRTU")));
#endif
	// ASCII directory name: ASRTU_CONFIG_DIR reaches Qt through the local
	// 8-bit environment, which cannot carry arbitrary Unicode on every
	// locale (Windows CI uses a Western ANSI codepage). Unicode station
	// values below still exercise the UTF-8 INI codec.
	const QString root =
	    temporary.filePath(QStringLiteral("config directory"));
	qputenv("ASRTU_CONFIG_DIR", root.toLocal8Bit());
	QSettings::setDefaultFormat(QSettings::NativeFormat);
	const QByteArray geometry("\0\1\2\xff", 4);
	{
		asrtu::IniSettings launcher(QStringLiteral("launcher"));
		assert(launcher.format() == QSettings::IniFormat);
		assert(!launcher.fallbacksEnabled());
		assert(QFileInfo(launcher.fileName()).absoluteFilePath() ==
		       QFileInfo(root + QStringLiteral("/launcher.ini"))
			   .absoluteFilePath());
		launcher.setValue(QStringLiteral("nickname"),
				  QStringLiteral("测试局/日本語"));
		launcher.setValue(QStringLiteral("longitude"), 120.123456);
		launcher.setValue(QStringLiteral("sdr/path"),
				  QStringLiteral("C:/软件/SDR.exe"));
		launcher.beginWriteArray(QStringLiteral("sdr/custom"));
		launcher.setArrayIndex(0);
		launcher.setValue(QStringLiteral("name"),
				  QStringLiteral("接收机"));
		launcher.endArray();
		launcher.sync();
		assert(launcher.status() == QSettings::NoError);
		asrtu::IniSettings decoder(QStringLiteral("decoder"));
		decoder.setValue(QStringLiteral("geometry"), geometry);
		decoder.setValue(QStringLiteral("plots/loopSpectrumVisible"),
				 false);
	}
	assert(QFileInfo::exists(root + QStringLiteral("/launcher.ini")));
	{
		asrtu::IniSettings launcher(QStringLiteral("launcher"));
		assert(launcher.value(QStringLiteral("nickname")).toString() ==
		       QStringLiteral("测试局/日本語"));
		assert(launcher.value(QStringLiteral("longitude")).toDouble() ==
		       120.123456);
		assert(launcher.beginReadArray(QStringLiteral("sdr/custom")) ==
		       1);
		launcher.setArrayIndex(0);
		assert(launcher.value(QStringLiteral("name")).toString() ==
		       QStringLiteral("接收机"));
		launcher.endArray();
		asrtu::IniSettings decoder(QStringLiteral("decoder"));
		assert(
		    decoder.value(QStringLiteral("geometry")).toByteArray() ==
		    geometry);
		assert(
		    !decoder.value(QStringLiteral("plots/loopSpectrumVisible"))
			 .toBool());
		assert(!decoder.contains(QStringLiteral("nickname")));
	}
	const QString proxyPath =
	    temporary.filePath(QStringLiteral("config.cfg"));
	QFile proxy(proxyPath);
	assert(proxy.open(QIODevice::WriteOnly));
	proxy.write(
	    QStringLiteral(
		"proxy_nickname = \"地面站\";\nproxy_long = 123.5;\nproxy_lat "
		"= -12.5;\nproxy_alt = 42;\nsat_name = \"JAMX\";\n")
		.toUtf8());
	proxy.close();
	{
		asrtu::IniSettings launcher(QStringLiteral("launcher"));
		assert(asrtu::importProxySettings(proxyPath, launcher));
		assert(launcher.value(QStringLiteral("nickname")).toString() ==
		       QStringLiteral("地面站"));
		assert(launcher.value(QStringLiteral("latitude")).toDouble() ==
		       -12.5);
		assert(launcher.contains(QStringLiteral("sdr/path")));
		assert(!asrtu::importProxySettings(
		    proxyPath + QStringLiteral(".missing"), launcher));
	}
	// A blocked directory must report failure, never fall back to native
	// storage.
	qputenv("ASRTU_CONFIG_DIR", proxyPath.toLocal8Bit());
	asrtu::IniSettings blocked(QStringLiteral("launcher"));
	blocked.setValue(QStringLiteral("probe"), true);
	blocked.sync();
	assert(blocked.status() == QSettings::AccessError);
	return 0;
}
