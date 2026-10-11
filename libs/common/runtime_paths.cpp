#include "runtime_paths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace asrtu
{

QString moduleDirectory(const QString& module, const QString& executable,
                        const QString& applicationDirectory)
{
    const QDir current(applicationDirectory.isEmpty()
                           ? QCoreApplication::applicationDirPath()
                           : applicationDirectory);
    // Build trees and portable packages keep executables together.
    if (QFileInfo::exists(current.filePath(executable)))
        return current.absolutePath();
    const QDir sibling(current.absoluteFilePath(QStringLiteral("../") + module));
    if (QFileInfo::exists(sibling.filePath(executable)))
        return QDir::cleanPath(sibling.absolutePath());
    const QDir common(current.absoluteFilePath(QStringLiteral("../common")));
    if (QFileInfo::exists(common.filePath(executable)))
        return QDir::cleanPath(common.absolutePath());
    return current.absolutePath();
}

QString commonDirectory(const QString& applicationDirectory)
{
    const QDir current(applicationDirectory.isEmpty()
                           ? QCoreApplication::applicationDirPath()
                           : applicationDirectory);
    // The launcher directory is the shared runtime directory in installed
    // layouts.  Keeping this as the application directory also works for
    // flat build and portable layouts.
    return QDir::cleanPath(current.absolutePath());
}

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
