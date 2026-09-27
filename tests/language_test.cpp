#include "translation.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QTemporaryDir>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

int main(int argc, char **argv)
{
	QCoreApplication application(argc, argv);
	QTemporaryDir directory;
	assert(directory.isValid());
	const QStringList plain{QStringLiteral("launcher")};
	const auto resolve = [&](const QStringList &arguments,
				 QLocale::Language system) {
		return resolveApplicationLanguage(arguments, directory.path(),
						  system);
	};
	assert(resolve(plain, QLocale::Chinese) == QStringLiteral("zh"));
	assert(resolve(plain, QLocale::Japanese) == QStringLiteral("ja"));
	assert(resolve(plain, QLocale::German) == QStringLiteral("en"));
	QSettings installed(
	    QDir(directory.path()).filePath(QStringLiteral("ui-language.ini")),
	    QSettings::IniFormat);
	for (const QString &language :
	     {QStringLiteral("en"), QStringLiteral("ja"),
	      QStringLiteral("zh")}) {
		installed.setValue(QStringLiteral("UI/Language"), language);
		installed.sync();
		assert(resolve(plain, QLocale::Chinese) == language);
		assert(resolve(plain, QLocale::Japanese) == language);
		assert(resolve(plain, QLocale::English) == language);
	}
	assert(resolve({QStringLiteral("launcher"),
			QStringLiteral("--language=en")},
		       QLocale::Chinese) == QStringLiteral("en"));
	assert(resolve({QStringLiteral("launcher"),
			QStringLiteral("--language"), QStringLiteral("ja")},
		       QLocale::Chinese) == QStringLiteral("ja"));
	installed.setValue(QStringLiteral("UI/Language"),
			   QStringLiteral("invalid"));
	installed.sync();
	assert(resolve(plain, QLocale::Chinese) == QStringLiteral("zh"));
	return 0;
}
