#include "runtime_paths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace asrtu
{

QString recordsDirectory()
{
#ifdef Q_OS_WIN
	return QDir::cleanPath(
	    QDir(QCoreApplication::applicationDirPath())
		.absoluteFilePath(QStringLiteral("../ASRTU1_Records")));
#else
	QString dataDirectory = QStandardPaths::writableLocation(
	    QStandardPaths::GenericDataLocation);
	if (dataDirectory.isEmpty())
		dataDirectory =
		    QDir::home().filePath(QStringLiteral(".local/share"));
	return QDir(dataDirectory)
	    .filePath(QStringLiteral("ASRTU/ASRTU1_Records"));
#endif
}

QString proxyConfigPath()
{
#ifdef Q_OS_WIN
	return QDir::cleanPath(
	    QDir(QCoreApplication::applicationDirPath())
		.absoluteFilePath(QStringLiteral("../proxy/config.cfg")));
#else
	QString configDirectory =
	    QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
	if (configDirectory.isEmpty())
		configDirectory =
		    QDir::home().filePath(QStringLiteral(".config/ASRTU"));
	return QDir(configDirectory).filePath(QStringLiteral("config.cfg"));
#endif
}

} // namespace asrtu
