#pragma once

#include <QLocale>
#include <QStringList>

class QApplication;
class QTranslator;

QString resolveApplicationLanguage(const QStringList &arguments,
				   const QString &applicationDirectory,
				   QLocale::Language systemLanguage);
QString applicationLanguage();

bool installSystemTranslation(QApplication &application,
			      QTranslator &translator);
