#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace asrtu
{

// Always name a real INI file. Never use QSettings' platform-native backend
// (registry on Windows, CFPreferences on macOS) or its organization fallbacks.
inline QString settingsDirectory(const QString &applicationDirectory = {})
{
	const QString overridePath = qEnvironmentVariable("ASRTU_CONFIG_DIR");
	if (QDir::isAbsolutePath(overridePath))
		return QDir::cleanPath(overridePath);
#ifdef Q_OS_WIN
	QDir root(applicationDirectory.isEmpty()
		      ? QCoreApplication::applicationDirPath()
		      : applicationDirectory);
	if (root.dirName().compare(QStringLiteral("bin"),
				   Qt::CaseInsensitive) == 0 ||
	    root.dirName().compare(QStringLiteral("launcher"),
				   Qt::CaseInsensitive) == 0 ||
	    root.dirName().compare(QStringLiteral("decoder"),
				   Qt::CaseInsensitive) == 0 ||
	    root.dirName().compare(QStringLiteral("doppler"),
				   Qt::CaseInsensitive) == 0)
		root.cdUp();
	return root.filePath(QStringLiteral("config"));
#else
	Q_UNUSED(applicationDirectory);
	const QString base = QStandardPaths::writableLocation(
	    QStandardPaths::GenericConfigLocation);
	return QDir(base.isEmpty()
			? QDir::home().filePath(QStringLiteral(".config"))
			: base)
	    .filePath(QStringLiteral("ASRTU"));
#endif
}

inline QString settingsFile(const QString &name,
			    const QString &applicationDirectory = {})
{
	return QDir(settingsDirectory(applicationDirectory))
	    .filePath(name + QStringLiteral(".ini"));
}

class IniSettings final : public QSettings
{
      public:
	explicit IniSettings(const QString &name,
			     const QString &applicationDirectory = {})
	    : QSettings(settingsFile(name, applicationDirectory),
			QSettings::IniFormat)
	{
		setIniCodec("UTF-8");
		setFallbacksEnabled(false);
	}
	~IniSettings() override
	{
		sync();
		if (status() != QSettings::NoError)
			qWarning().noquote()
			    << "Unable to read/write INI settings:"
			    << fileName();
	}
};

} // namespace asrtu
