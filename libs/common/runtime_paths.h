#pragma once

#include <QString>

namespace asrtu
{

QString recordsDirectory();
QString proxyConfigPath();
QString moduleDirectory(const QString& module, const QString& executable,
                        const QString& applicationDirectory = {});
QString commonDirectory(const QString& applicationDirectory = {});

} // namespace asrtu
