#include "runtime_paths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    QTemporaryDir temporary;
    assert(temporary.isValid());
    QDir root(temporary.path());
    assert(root.mkpath("launcher"));
    const QString launcher = root.filePath("launcher");
    for (const QString& module : {QStringLiteral("decoder"), QStringLiteral("doppler"),
                                  QStringLiteral("satnogs-uploader")}) {
        assert(root.mkpath(module));
        const QString executable = module + QStringLiteral(".exe");
        QFile binary(QDir(root.filePath(module)).filePath(executable));
        assert(binary.open(QIODevice::WriteOnly));
        binary.close();
        assert(asrtu::moduleDirectory(module, executable, launcher) == root.filePath(module));
        QFile flat(QDir(launcher).filePath(executable));
        assert(flat.open(QIODevice::WriteOnly));
        flat.close();
        assert(asrtu::moduleDirectory(module, executable, launcher) == launcher);
    }
    assert(asrtu::moduleDirectory("missing", "missing.exe", launcher) == launcher);
    return 0;
}
