#include "runtime_paths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>

#include <iostream>

int main(int argc, char *argv[])
{
	QCoreApplication application(argc, argv);
	QCoreApplication::setOrganizationName(QStringLiteral("ASRTU"));
	QCoreApplication::setApplicationName(
	    QStringLiteral("RuntimePathsTest"));

	const QString dataHome = qEnvironmentVariable("XDG_DATA_HOME");
	const QString expected =
	    QDir(dataHome).filePath(QStringLiteral("ASRTU/ASRTU1_Records"));
	const QString records = asrtu::recordsDirectory();
	if (records != expected) {
		std::cerr << "Unexpected records directory: "
			  << records.toUtf8().constData() << '\n';
		return 1;
	}

	const QString probeDirectory =
	    QDir(records).filePath(QStringLiteral("runtime-paths-test"));
	if (!QDir().mkpath(probeDirectory)) {
		std::cerr << "Unable to create records directory\n";
		return 1;
	}
	QFile probe(
	    QDir(probeDirectory).filePath(QStringLiteral("write-probe")));
	if (!probe.open(QIODevice::WriteOnly)) {
		std::cerr << "Records directory is not writable\n";
		return 1;
	}
	probe.write("ok");
	return 0;
}
