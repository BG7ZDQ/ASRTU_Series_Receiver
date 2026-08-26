#include "translation.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QLocale>
#include <QTranslator>

bool installSystemTranslation(QApplication& application,
                              QTranslator& translator)
{
    const QStringList arguments = QCoreApplication::arguments();
    const bool forceEnglish = arguments.contains(QStringLiteral("--language=en"));
    const bool forceChinese = arguments.contains(QStringLiteral("--language=zh"));
    const bool forceJapanese = arguments.contains(QStringLiteral("--language=ja"));
    if (forceChinese ||
        (!forceEnglish && !forceJapanese &&
         QLocale::system().language() == QLocale::Chinese))
        return false;

    const bool japanese = forceJapanese ||
        (!forceEnglish && QLocale::system().language() == QLocale::Japanese);
    const QString translation = QDir(QCoreApplication::applicationDirPath())
                                    .filePath(japanese
                                        ? QStringLiteral("translations/asrtu_ja.qm")
                                        : QStringLiteral("translations/asrtu_en.qm"));
    if (!translator.load(translation))
        return false;
    return application.installTranslator(&translator);
}
