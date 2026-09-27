#include "translation.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QLocale>
#include <QSettings>
#include <QTranslator>

QString resolveApplicationLanguage(const QStringList &arguments,
				   const QString &applicationDirectory,
				   QLocale::Language systemLanguage)
{
	const QStringList supported{QStringLiteral("zh"), QStringLiteral("en"),
				    QStringLiteral("ja")};
	for (int i = 1; i < arguments.size(); ++i) {
		QString value;
		if (arguments.at(i).startsWith(QStringLiteral("--language=")))
			value = arguments.at(i).mid(11);
		else if (arguments.at(i) == QStringLiteral("--language") &&
			 i + 1 < arguments.size())
			value = arguments.at(++i);
		if (supported.contains(value))
			return value;
	}
	QSettings installed(QDir(applicationDirectory)
				.filePath(QStringLiteral("ui-language.ini")),
			    QSettings::IniFormat);
	const QString saved =
	    installed.value(QStringLiteral("UI/Language")).toString();
	if (supported.contains(saved))
		return saved;
	if (systemLanguage == QLocale::Chinese)
		return QStringLiteral("zh");
	if (systemLanguage == QLocale::Japanese)
		return QStringLiteral("ja");
	return QStringLiteral("en");
}

QString applicationLanguage()
{
	return resolveApplicationLanguage(
	    QCoreApplication::arguments(),
	    QCoreApplication::applicationDirPath(),
	    QLocale::system().language());
}

bool installSystemTranslation(QApplication &application,
			      QTranslator &translator)
{
	const QString language = applicationLanguage();
	if (language == QStringLiteral("zh"))
		return false;

	const bool japanese = language == QStringLiteral("ja");
	const QString translation =
	    QDir(QCoreApplication::applicationDirPath())
		.filePath(japanese
			      ? QStringLiteral("translations/asrtu_ja.qm")
			      : QStringLiteral("translations/asrtu_en.qm"));
	if (!translator.load(translation))
		return false;
	return application.installTranslator(&translator);
}
