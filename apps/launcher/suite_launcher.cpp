#ifdef __APPLE__
#include <libproc.h>
#endif
#include "audio_devices.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDataStream>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFileIconProvider>
#include <QFileDialog>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardItemModel>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>
#include <QProcess>
#include <QProcessEnvironment>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QSaveFile>
#include <QStandardPaths>
#include <QScrollArea>
#include <QSettings>
#include <QScreen>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QStyle>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QTranslator>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>
#include <QWidget>

#include "translation.h"
#include "runtime_paths.h"
#include "version.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#ifdef Q_OS_WIN
#include <windows.h>
#include <mmsystem.h>
#include <tlhelp32.h>
#else
#include <cerrno>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace
{
QString executableName(const QString &baseName)
{
#ifdef Q_OS_WIN
	return baseName + QStringLiteral(".exe");
#else
	return baseName;
#endif
}

QString explicitLanguageArgument()
{
	return QStringLiteral("--language=") + applicationLanguage();
}

void notifyRunningDspAudioDevice(int deviceId)
{
	QLocalSocket socket;
	socket.connectToServer(QStringLiteral("ASRTU_DSP_CONTROL_V1"),
			       QIODevice::WriteOnly);
	if (!socket.waitForConnected(40))
		return;
	socket.write(QStringLiteral("audio-device:%1").arg(deviceId).toUtf8());
	socket.flush();
	socket.waitForBytesWritten(40);
	socket.disconnectFromServer();
}

QString decoderDirectory()
{
	return asrtu::moduleDirectory(
	    QStringLiteral("decoder"),
	    executableName(QStringLiteral("ASRTU1_Demod_CQt")));
}

bool sdrProcessRunning(const QString &executable)
{
	if (executable.isEmpty())
		return false;
	const QString name = QFileInfo(executable).fileName();
#ifdef Q_OS_WIN
	const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE)
		return false;
	PROCESSENTRY32W entry{};
	entry.dwSize = sizeof(entry);
	bool found = false;
	if (Process32FirstW(snapshot, &entry)) {
		do {
			if (name.compare(
				QString::fromWCharArray(entry.szExeFile),
				Qt::CaseInsensitive) == 0) {
				found = true;
				break;
			}
		} while (Process32NextW(snapshot, &entry));
	}
	CloseHandle(snapshot);
	return found;
#elif defined(Q_OS_MACOS)
	const QString target = QFileInfo(executable).canonicalFilePath();
	if (target.isEmpty())
		return false;
	const int bytes = proc_listpids(PROC_ALL_PIDS, 0, nullptr, 0);
	if (bytes <= 0)
		return false;
	QVector<pid_t> pids(bytes / int(sizeof(pid_t)) + 32);
	const int count = proc_listpids(PROC_ALL_PIDS, 0, pids.data(),
					pids.size() * int(sizeof(pid_t))) /
			  int(sizeof(pid_t));
	for (int index = 0; index < count; ++index) {
		char path[PROC_PIDPATHINFO_MAXSIZE] = {};
		if (proc_pidpath(pids[index], path, sizeof(path)) > 0 &&
		    QFileInfo(QString::fromUtf8(path)).canonicalFilePath() ==
			target)
			return true;
	}
	return false;
#else
	const QString target = QFileInfo(executable).canonicalFilePath();
	if (target.isEmpty())
		return false;
	const QStringList entries =
	    QDir(QStringLiteral("/proc"))
		.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
	for (const QString &entry : entries) {
		bool numeric = false;
		entry.toInt(&numeric);
		if (!numeric)
			continue;
		const QString link =
		    QFileInfo(QStringLiteral("/proc/%1/exe").arg(entry))
			.canonicalFilePath();
		if (!link.isEmpty() && link == target)
			return true;
	}
	return false;
#endif
}

QString audioConverterPath()
{
	const QString local =
	    QDir(decoderDirectory())
		.filePath(executableName(QStringLiteral("sndfile-convert")));
	if (QFileInfo::exists(local))
		return local;
	return QStandardPaths::findExecutable(
	    QStringLiteral("sndfile-convert"));
}

QString proxyDirectory()
{
#ifdef Q_OS_WIN
	return QCoreApplication::applicationDirPath() +
	       QStringLiteral("/../proxy");
#else
	return QFileInfo(asrtu::proxyConfigPath()).absolutePath();
#endif
}

QString sdrSharpDirectory()
{
	const QDir current(QCoreApplication::applicationDirPath());
	const QString sibling =
	    current.filePath(QStringLiteral("sdrsharp/SDRSharp.exe"));
	if (QFileInfo::exists(sibling))
		return QDir::cleanPath(
		    current.filePath(QStringLiteral("sdrsharp")));
	return QDir::cleanPath(current.filePath(QStringLiteral("../sdrsharp")));
}

struct SdrProgram {
	QString name;
	QString executable;
	QStringList arguments;
	QString workingDirectory;
	bool sdrSharp = false;
	QString overrideKey; // "sdrsharp"/"sdrpp" for editable built-in entries
	int customIndex = -1; // array index for user-added entries
};

void applySdrProgramOverride(SdrProgram *program)
{
	if (program->overrideKey.isEmpty())
		return;
	QSettings settings;
	settings.beginGroup(QStringLiteral("sdr/programOverrides/") +
			    program->overrideKey);
	program->name =
	    settings.value(QStringLiteral("name"), program->name).toString();
	program->executable =
	    settings.value(QStringLiteral("executable"), program->executable)
		.toString();
	program->arguments =
	    settings.value(QStringLiteral("arguments"), program->arguments)
		.toStringList();
	program->workingDirectory =
	    QFileInfo(program->executable).absolutePath();
	settings.endGroup();
}

QString sdrProgramCommandLine(const SdrProgram &program)
{
	QString command = QDir::toNativeSeparators(program.executable);
	if (command.contains(QLatin1Char(' ')))
		command = QLatin1Char('"') + command + QLatin1Char('"');
	for (const QString &argument : program.arguments) {
		command += QLatin1Char(' ');
		if (argument.contains(QLatin1Char(' ')))
			command +=
			    QLatin1Char('"') + argument + QLatin1Char('"');
		else
			command += argument;
	}
	return command;
}

QString sdrppBundledDirectory()
{
#ifdef Q_OS_WIN
	const QString local = QProcessEnvironment::systemEnvironment().value(
	    QStringLiteral("LOCALAPPDATA"),
	    QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
	const QString userInstall =
	    QDir(local).filePath(QStringLiteral("ASRTU Series Receiver/sdrpp"));
	if (QFileInfo::exists(
		QDir(userInstall).filePath(QStringLiteral("sdrpp.exe"))))
		return userInstall;
#endif
	return QDir(QCoreApplication::applicationDirPath())
	    .filePath(QStringLiteral("../sdrpp"));
}

bool isSdrppInstallation(const QString &executable)
{
	const QFileInfo exe(executable);
	if (!exe.isFile())
		return false;
	const QDir root(exe.absolutePath());
	// A copied SDR++ executable without its resource tree starts and then
	// crashes while loading themes/modules. Reject that stale installation
	// so the chooser can continue to the complete bundled copy.
#ifdef Q_OS_WIN
	return QFileInfo::exists(
		   root.filePath(QStringLiteral("sdrpp_core.dll"))) &&
	       QFileInfo::exists(root.filePath(QStringLiteral("modules"))) &&
	       QFileInfo::exists(
		   root.filePath(QStringLiteral("res/themes/dark.json"))) &&
	       QFileInfo::exists(
		   root.filePath(QStringLiteral("res/colormaps/classic.json")));
#else
	// System packages store shared resources under /usr/share, not bin/.
	return exe.isExecutable();
#endif
}

QStringList sdrppCandidates()
{
	QStringList candidates;
	const QDir current(QCoreApplication::applicationDirPath());
	// Portable and installed layouts keep SDR++ beside the launcher, just
	// like SDR#. Check these locations before any stale per-user copy.
	candidates << current.filePath(QStringLiteral("sdrpp/") +
				       executableName(QStringLiteral("sdrpp")));
	candidates << current.filePath(QStringLiteral("../sdrpp/") +
				       executableName(QStringLiteral("sdrpp")));
	const QString saved =
	    QSettings().value(QStringLiteral("sdr/lastExecutable")).toString();
	if (QFileInfo(saved).fileName().compare(
		executableName(QStringLiteral("sdrpp")), Qt::CaseInsensitive) ==
	    0)
		candidates << saved;
	candidates << QDir(sdrppBundledDirectory())
			  .filePath(executableName(QStringLiteral("sdrpp")));
	const QString discovered = QStandardPaths::findExecutable(
	    executableName(QStringLiteral("sdrpp")));
	if (!discovered.isEmpty())
		candidates << discovered;
	candidates.removeDuplicates();
	return candidates;
}

QList<SdrProgram> availableSdrPrograms()
{
	QList<SdrProgram> programs;
	const QString sharp =
	    QDir(sdrSharpDirectory()).filePath(QStringLiteral("SDRSharp.exe"));
	const QString sharpCanonical = QFileInfo(sharp).canonicalFilePath();
	SdrProgram sharpProgram{
	    QStringLiteral("SDR#"),	sharp, {}, sdrSharpDirectory(), true,
	    QStringLiteral("sdrsharp"), -1};
	applySdrProgramOverride(&sharpProgram);
	programs.append(sharpProgram);
	bool sdrppFound = false;
	for (const QString &candidate : sdrppCandidates()) {
		if (!isSdrppInstallation(candidate))
			continue;
		const QString canonical =
		    QFileInfo(candidate).canonicalFilePath();
		if (!sharpCanonical.isEmpty() && canonical == sharpCanonical)
			continue;
		if (QFileInfo(candidate).fileName().compare(
			QStringLiteral("SDRSharp.exe"), Qt::CaseInsensitive) ==
		    0)
			continue;
		const QString directory = QFileInfo(candidate).absolutePath();
		SdrProgram sdrppProgram{
		    QStringLiteral("SDR++"), candidate, {}, directory, false,
		    QStringLiteral("sdrpp"), -1};
		applySdrProgramOverride(&sdrppProgram);
		programs.append(sdrppProgram);
		sdrppFound = true;
		break;
	}
	if (!sdrppFound) {
		SdrProgram placeholder{
		    QStringLiteral("SDR++"), {}, {}, {}, false,
		    QStringLiteral("sdrpp"), -1};
		applySdrProgramOverride(&placeholder);
		programs.append(placeholder);
	}
	QSettings settings;
	const int customCount =
	    settings.beginReadArray(QStringLiteral("sdr/customPrograms"));
	for (int i = 0; i < customCount; ++i) {
		settings.setArrayIndex(i);
		const QString name =
		    settings.value(QStringLiteral("name")).toString().trimmed();
		const QString executable =
		    settings.value(QStringLiteral("executable")).toString();
		if (name.isEmpty())
			continue;
		programs.append(
		    {name,
		     executable,
		     settings.value(QStringLiteral("arguments")).toStringList(),
		     QFileInfo(executable).absolutePath(),
		     false,
		     {},
		     i});
	}
	settings.endArray();
	return programs;
}

bool sdrExecutableAvailable(const QString &path);

#ifdef Q_OS_WIN
bool sdrppBridgeAvailable()
{
	for (const QString &candidate : sdrppCandidates()) {
		if (!isSdrppInstallation(candidate))
			continue;
		if (QFileInfo::exists(QDir(QFileInfo(candidate).absolutePath())
					  .filePath(QStringLiteral(
					      "modules/asrtu_sink.dll"))))
			return true;
	}
	// Also honor a path edited in the SDR chooser.
	for (const SdrProgram &program : availableSdrPrograms()) {
		if (program.overrideKey != QStringLiteral("sdrpp") ||
		    !isSdrppInstallation(program.executable))
			continue;
		if (QFileInfo::exists(QDir(program.workingDirectory)
					  .filePath(QStringLiteral(
					      "modules/asrtu_sink.dll"))))
			return true;
	}
	return false;
}

bool sdrSharpBridgeAvailable()
{
	for (const SdrProgram &program : availableSdrPrograms()) {
		if (program.overrideKey != QStringLiteral("sdrsharp") ||
		    !sdrExecutableAvailable(program.executable))
			continue;
		return QFileInfo::exists(
		    QDir(program.workingDirectory)
			.filePath(QStringLiteral(
			    "Plugins/SDRSharp.AstroSeriesBridge.dll")));
	}
	return false;
}
#endif

bool sdrExecutableAvailable(const QString &path)
{
	const QFileInfo info(path);
	// Let CreateProcess validate Windows executables; Unix also requires
	// executable permission before offering the program in the chooser.
#ifdef Q_OS_WIN
	return info.isFile();
#else
	return info.isFile() && info.isExecutable();
#endif
}

int normalizedInputMode(int mode)
{
#ifdef Q_OS_WIN
	// SDR# and SDR++ publish the same V1 I/Q mapping. Keep mode 3 as a
	// compatibility alias for settings saved by older launcher versions.
	if (mode == 3)
		return 2;
#endif
	return mode;
}

QIcon sdrProgramIcon(const SdrProgram &program)
{
	if (program.sdrSharp || program.name.compare(QStringLiteral("SDR#"),
						     Qt::CaseInsensitive) == 0)
		return QIcon(QStringLiteral(":/launcher/sdrsharp.png"));
	if (program.name.compare(QStringLiteral("SDR++"),
				 Qt::CaseInsensitive) == 0)
		return QIcon(QStringLiteral(":/launcher/sdrpp.png"));
	QIcon icon = QFileIconProvider().icon(QFileInfo(program.executable));
	if (icon.isNull())
		icon = QApplication::style()->standardIcon(
		    QStyle::SP_ComputerIcon);
	return icon;
}

bool readJsonObject(const QString &path, QJsonObject *object, QString *error)
{
	*object = {};
	QFile file(path);
	if (!file.exists())
		return true;
	if (!file.open(QIODevice::ReadOnly)) {
		*error = QCoreApplication::translate(
			     "ASRTU", "无法读取 SDR++ 设置：\n%1")
			     .arg(path);
		return false;
	}
	QJsonParseError parseError{};
	const QJsonDocument document =
	    QJsonDocument::fromJson(file.readAll(), &parseError);
	if (parseError.error != QJsonParseError::NoError ||
	    !document.isObject()) {
		*error = QCoreApplication::translate(
			     "ASRTU", "SDR++ 配置文件格式有误：\n%1\n%2")
			     .arg(path, parseError.errorString());
		return false;
	}
	*object = document.object();
	return true;
}

bool writeJsonObject(const QString &path, const QJsonObject &object,
		     QString *error)
{
	QSaveFile file(path);
	if (!file.open(QIODevice::WriteOnly)) {
		*error = QCoreApplication::translate(
			     "ASRTU", "无法保存 SDR++ 设置：\n%1")
			     .arg(path);
		return false;
	}
	file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
	if (!file.commit()) {
		*error = QCoreApplication::translate(
			     "ASRTU", "无法保存 SDR++ 设置：\n%1")
			     .arg(path);
		return false;
	}
	return true;
}

bool configureSdrppPreset(const QString &sdrppRoot, QString *error)
{
	const QDir root(sdrppRoot);
	if (!QFileInfo::exists(
		root.filePath(QStringLiteral("modules/asrtu_sink.dll"))))
		return true;

	const QString configFile = root.filePath(QStringLiteral("config.json"));
	const QString radioFile =
	    root.filePath(QStringLiteral("radio_config.json"));
	const QString bridgeFile =
	    root.filePath(QStringLiteral("asrtu_sink_config.json"));
	const QString markerFile =
	    root.filePath(QStringLiteral("asrtu-preset.version"));
	QJsonObject config;
	QJsonObject radioConfig;
	QJsonObject bridgeConfig;
	if (!readJsonObject(configFile, &config, error) ||
	    !readJsonObject(radioFile, &radioConfig, error) ||
	    !readJsonObject(bridgeFile, &bridgeConfig, error))
		return false;

	// Preserve settings changed by the operator after the one-time
	// migration.
	constexpr int kPresetVersion = 3;
	bool presetAlreadyApplied = false;
	{
		// Close the read handle before QSaveFile replaces this same
		// path; Windows otherwise reports a sharing violation on
		// migration.
		QFile marker(markerFile);
		presetAlreadyApplied =
		    marker.open(QIODevice::ReadOnly) &&
		    marker.readAll().trimmed().toInt() >= kPresetVersion;
	}
	// The +5 kHz IF offset only works with normal (VFO) tuning. SDR++'s
	// centerTuning mode snaps the hardware back to the VFO frequency and
	// makes the sink retune the hardware on every frame, so repair old
	// installs even when the rest of the preset is already present.
	if (presetAlreadyApplied) {
		if (!config.value(QStringLiteral("centerTuning")).toBool(true))
			return true;
		config[QStringLiteral("centerTuning")] = false;
		return writeJsonObject(configFile, config, error);
	}

	QJsonObject instances =
	    config.value(QStringLiteral("moduleInstances")).toObject();
	QString bridgeName = QStringLiteral("ASRTU Bridge");
	for (auto it = instances.constBegin(); it != instances.constEnd();
	     ++it) {
		if (it.value()
			.toObject()
			.value(QStringLiteral("module"))
			.toString() == QStringLiteral("asrtu_sink")) {
			bridgeName = it.key();
			break;
		}
	}
	QJsonObject bridge = instances.value(bridgeName).toObject();
	bridge[QStringLiteral("enabled")] = true;
	bridge[QStringLiteral("module")] = QStringLiteral("asrtu_sink");
	instances[bridgeName] = bridge;
	if (!instances.value(QStringLiteral("Radio")).isObject()) {
		instances[QStringLiteral("Radio")] = QJsonObject{
		    {QStringLiteral("enabled"), true},
		    {QStringLiteral("module"), QStringLiteral("radio")}};
	}
	config[QStringLiteral("moduleInstances")] = instances;

	QJsonObject streams =
	    config.value(QStringLiteral("streams")).toObject();
	QJsonObject radio = streams.value(QStringLiteral("Radio")).toObject();
	// SDR++ loads these keys unconditionally; a missing key becomes JSON
	// null and aborts sink initialization.
	if (!radio.contains(QStringLiteral("volume")) ||
	    radio.value(QStringLiteral("volume")).isNull())
		radio[QStringLiteral("volume")] = 1.0;
	if (!radio.contains(QStringLiteral("muted")) ||
	    radio.value(QStringLiteral("muted")).isNull())
		radio[QStringLiteral("muted")] = false;
	radio[QStringLiteral("sink")] = QStringLiteral("ASRTU Receiver");
	streams[QStringLiteral("Radio")] = radio;
	config[QStringLiteral("streams")] = streams;

	config[QStringLiteral("centerTuning")] = false;
	QJsonObject radioSettings =
	    radioConfig.value(QStringLiteral("Radio")).toObject();
	radioSettings[QStringLiteral("selectedDemodId")] =
	    7; // RAW in SDR++ 1.3.0
	QJsonObject raw = radioSettings.value(QStringLiteral("RAW")).toObject();
	raw[QStringLiteral("bandwidth")] = 48000.0;
	radioSettings[QStringLiteral("RAW")] = raw;
	radioConfig[QStringLiteral("Radio")] = radioSettings;

	QJsonObject bridgeSettings =
	    bridgeConfig.value(QStringLiteral("Radio")).toObject();
	bridgeSettings[QStringLiteral("autoDoppler")] = true;
	bridgeConfig[QStringLiteral("Radio")] = bridgeSettings;

	// Commit the marker last so a failed auxiliary write is retried next
	// time.
	if (!writeJsonObject(radioFile, radioConfig, error) ||
	    !writeJsonObject(bridgeFile, bridgeConfig, error))
		return false;
	if (!writeJsonObject(configFile, config, error))
		return false;
	QSaveFile markerOutput(markerFile);
	if (!markerOutput.open(QIODevice::WriteOnly)) {
		*error = QCoreApplication::translate(
			     "ASRTU", "无法保存 SDR++ 设置：\n%1")
			     .arg(markerFile);
		return false;
	}
	markerOutput.write(QByteArray::number(kPresetVersion));
	if (!markerOutput.commit()) {
		*error = QCoreApplication::translate(
			     "ASRTU", "无法保存 SDR++ 设置：\n%1")
			     .arg(markerFile);
		return false;
	}
	return true;
}

bool makeSdrCommandProgram(const QString &command, SdrProgram *program)
{
	QStringList parts;
	if (QFileInfo(command.trimmed()).isFile())
		parts << command.trimmed();
	else
		parts = QProcess::splitCommand(command.trimmed());
	if (parts.isEmpty())
		return false;
	QString executable = parts.first();
	if (!QFileInfo(executable).isFile())
		executable = QStandardPaths::findExecutable(executable);
	if (!sdrExecutableAvailable(executable))
		return false;
	program->executable = QFileInfo(executable).absoluteFilePath();
	program->arguments = parts.mid(1);
	program->workingDirectory = QFileInfo(executable).absolutePath();
	program->sdrSharp = false;
	return true;
}

QString configPath()
{
	return asrtu::proxyConfigPath();
}

struct SatelliteProfile {
	QString name;
	QString webSocket;
	int noradId;
};

constexpr int kAudioChannelCountRole = Qt::UserRole + 1;

struct AudioInputDevice {
	QString name;
	int id = -1;
	int channels = 0;
};

QList<SatelliteProfile> satelliteProfiles()
{
	return {{QStringLiteral("ASRTU-1"), QStringLiteral("ws://1.92.100.130"),
		 61781},
		{QStringLiteral("BY-04uv"),
		 QStringLiteral("ws://119.45.229.166"), 98247},
		// JAMX has no MMT upload server; telemetry is submitted by the
		// native SatNOGS uploader, so the WebSocket target stays empty.
		{QStringLiteral("JAMX"), QString(), 100470}};
}

int noradForSatellite(const QString &name)
{
	for (const auto &profile : satelliteProfiles()) {
		if (profile.name.compare(name, Qt::CaseInsensitive) == 0)
			return profile.noradId;
	}
	return 0;
}

QString webSocketForSatellite(const QString &name)
{
	for (const auto &profile : satelliteProfiles()) {
		if (profile.name.compare(name, Qt::CaseInsensitive) == 0)
			return profile.webSocket;
	}
	return satelliteProfiles().first().webSocket;
}

QString recordsDirectory()
{
	return asrtu::recordsDirectory();
}

QString escapedConfigString(QString value)
{
	value.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
	value.replace(QLatin1Char('"'), QStringLiteral("\\\""));
	return value;
}

QString createSessionDirectory(QString *error)
{
	const QString directory =
	    QDir(recordsDirectory())
		.filePath(QDateTime::currentDateTime().toString(
		    QStringLiteral("yyyyMMdd_HHmmss_zzz")));
	if (!QDir().mkpath(directory)) {
		*error = QCoreApplication::translate(
			     "ASRTU", "无法创建录音和日志目录：\n%1")
			     .arg(directory);
		return {};
	}
	return QDir(directory).absolutePath();
}

QString compactSessionDirectory(const QString &directory)
{
	return QStringLiteral("/%1").arg(QFileInfo(directory).fileName());
}

int wavChannelCount(const QString &path)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
		return 0;
	QDataStream stream(&file);
	stream.setByteOrder(QDataStream::LittleEndian);
	char riff[4] = {};
	char wave[4] = {};
	quint32 riffSize = 0;
	if (stream.readRawData(riff, 4) != 4)
		return 0;
	stream >> riffSize;
	if (stream.readRawData(wave, 4) != 4 || QByteArray(riff, 4) != "RIFF" ||
	    QByteArray(wave, 4) != "WAVE")
		return 0;
	Q_UNUSED(riffSize)
	while (!stream.atEnd()) {
		char chunkId[4] = {};
		quint32 chunkSize = 0;
		if (stream.readRawData(chunkId, 4) != 4)
			break;
		stream >> chunkSize;
		if (QByteArray(chunkId, 4) == "fmt " && chunkSize >= 4) {
			quint16 format = 0;
			quint16 channels = 0;
			stream >> format >> channels;
			Q_UNUSED(format)
			return int(channels);
		}
		const qint64 skip = qint64(chunkSize) + (chunkSize & 1U);
		if (!file.seek(file.pos() + skip))
			break;
	}
	return 0;
}

bool startProgram(const QString &executable, const QStringList &arguments,
		  const QString &workingDirectory, const QString &outputLog,
		  bool visibleConsole, qint64 *processId, QString *error)
{
	Q_UNUSED(visibleConsole)
	if (!QFileInfo::exists(executable)) {
		*error =
		    QCoreApplication::translate("ASRTU", "找不到程序：\n%1")
			.arg(executable);
		return false;
	}
	QProcess process;
	process.setProgram(executable);
	process.setArguments(arguments);
	process.setWorkingDirectory(workingDirectory);
	if (!outputLog.isEmpty()) {
		process.setStandardOutputFile(outputLog, QIODevice::Append);
		process.setStandardErrorFile(outputLog, QIODevice::Append);
	}
	{
		QProcessEnvironment environment =
		    QProcessEnvironment::systemEnvironment();
		const QString common = asrtu::commonDirectory();
		const QString path = environment.value(QStringLiteral("PATH"));
		environment.insert(QStringLiteral("PATH"),
				   common + QDir::listSeparator() + path);
		environment.insert(QStringLiteral("TERM"),
				   QStringLiteral("xterm"));
		environment.insert(QStringLiteral("TERMINFO"),
				   QDir(workingDirectory)
				       .filePath(QStringLiteral("terminfo")));
		process.setProcessEnvironment(environment);
#ifdef Q_OS_WIN
		process.setCreateProcessArgumentsModifier(
		    [visibleConsole](QProcess::CreateProcessArguments *args) {
			    if (visibleConsole) {
				    args->flags &= ~CREATE_NO_WINDOW;
				    args->flags |= CREATE_NEW_CONSOLE;
			    } else {
				    args->flags &= ~CREATE_NEW_CONSOLE;
				    args->flags |= CREATE_NO_WINDOW;
			    }
		    });
#endif
	}
	if (!process.startDetached(processId)) {
		*error = QCoreApplication::translate("ASRTU", "无法启动：\n%1")
			     .arg(executable);
		return false;
	}
	return true;
}

bool startSdrProgram(const SdrProgram &program, qint64 *processId,
		     QString *error)
{
	if (sdrProcessRunning(program.executable)) {
		*error = QCoreApplication::translate(
		    "ASRTU",
		    "该 SDR "
		    "软件已在运行；请先关闭已有实例，避免重复启动桥接。");
		return false;
	}
	if (program.overrideKey == QStringLiteral("sdrpp") &&
	    !configureSdrppPreset(program.workingDirectory, error)) {
		return false;
	}
	QProcess process;
	process.setProgram(program.executable);
	process.setArguments(program.arguments);
	process.setWorkingDirectory(program.workingDirectory);
	QProcessEnvironment environment =
	    QProcessEnvironment::systemEnvironment();
	const QString path = environment.value(QStringLiteral("PATH"));
	environment.insert(QStringLiteral("PATH"), program.workingDirectory +
						       QDir::listSeparator() +
						       path);
	const QString soapyPlugins =
	    QDir(program.workingDirectory)
		.filePath(QStringLiteral("lib/SoapySDR/modules0.8"));
	if (QFileInfo::exists(soapyPlugins)) {
		// Keep the portable Soapy installation isolated from any system
		// or conda copy. This avoids duplicate plugin registration.
		environment.insert(QStringLiteral("SOAPY_SDR_ROOT"),
				   program.workingDirectory);
		environment.remove(QStringLiteral("SOAPY_SDR_PLUGIN_PATH"));
	}
	process.setProcessEnvironment(environment);
	if (!sdrExecutableAvailable(program.executable) ||
	    !process.startDetached(processId)) {
		*error = QCoreApplication::translate("ASRTU", "无法启动：\n%1")
			     .arg(program.executable) +
			 QLatin1Char('\n') + process.errorString();
		return false;
	}
	return true;
}

bool editSdrProgramDialog(QWidget *parent, SdrProgram *program, bool adding)
{
	QDialog dialog(parent);
	dialog.setObjectName(QStringLiteral("addSdrDialog"));
	dialog.setWindowTitle(QCoreApplication::translate(
	    "ASRTU", adding ? "添加 SDR 接收软件" : "修改 SDR 接收软件"));
	dialog.resize(560, 250);
	auto *layout = new QVBoxLayout(&dialog);
	auto *form = new QFormLayout;
	auto *name = new QLineEdit(program->name, &dialog);
	name->setObjectName(QStringLiteral("sdrName"));
	name->setMaxLength(60);
	name->setPlaceholderText(
	    QCoreApplication::translate("ASRTU", "可留空，默认使用程序名"));
	auto *path = new QLineEdit(&dialog);
	path->setObjectName(QStringLiteral("sdrPath"));
	if (!program->executable.isEmpty())
		path->setText(sdrProgramCommandLine(*program));
	path->setPlaceholderText(QCoreApplication::translate(
	    "ASRTU", "选择程序路径，也可填写启动命令"));
	auto *browse = new QPushButton(
	    QCoreApplication::translate("ASRTU", "浏览…"), &dialog);
	auto *pathRow = new QHBoxLayout;
	pathRow->addWidget(path, 1);
	pathRow->addWidget(browse);
	form->addRow(QCoreApplication::translate("ASRTU", "备注名（可留空）"),
		     name);
	form->addRow(QCoreApplication::translate("ASRTU", "程序路径或命令"),
		     pathRow);
	layout->addLayout(form);
	auto *error = new QLabel(&dialog);
	error->setObjectName(QStringLiteral("sdrValidationError"));
	error->setWordWrap(true);
	error->setStyleSheet(QStringLiteral("color:#b42318;"));
	layout->addWidget(error);
	auto *actions = new QDialogButtonBox(
	    QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
	layout->addWidget(actions);
	QObject::connect(browse, &QPushButton::clicked, &dialog, [&] {
		const QString selected = QFileDialog::getOpenFileName(
		    &dialog,
		    QCoreApplication::translate("ASRTU", "选择 SDR 接收软件"));
		if (!selected.isEmpty())
			path->setText(QDir::toNativeSeparators(selected));
	});
	QObject::connect(actions, &QDialogButtonBox::rejected, &dialog,
			 &QDialog::reject);
	QObject::connect(actions, &QDialogButtonBox::accepted, &dialog, [&] {
		SdrProgram parsed;
		if (!makeSdrCommandProgram(path->text(), &parsed)) {
			error->setText(QCoreApplication::translate(
			    "ASRTU", "找不到可执行程序，请检查路径或命令。"));
			path->setFocus();
			return;
		}
		QString chosenName = name->text().trimmed();
		if (chosenName.isEmpty()) {
			chosenName =
			    QFileInfo(parsed.executable).completeBaseName();
			if (chosenName.isEmpty())
				chosenName =
				    QFileInfo(parsed.executable).fileName();
		}
		if (chosenName.isEmpty()) {
			error->setText(QCoreApplication::translate(
			    "ASRTU", "请填写备注名或有效的程序路径。"));
			name->setFocus();
			return;
		}
		for (const auto &existing : availableSdrPrograms()) {
			if (existing.name.compare(chosenName,
						  Qt::CaseInsensitive) != 0)
				continue;
			const bool self =
			    !adding &&
			    ((!program->overrideKey.isEmpty() &&
			      existing.overrideKey == program->overrideKey) ||
			     (program->customIndex >= 0 &&
			      existing.customIndex == program->customIndex));
			if (!self) {
				error->setText(QCoreApplication::translate(
				    "ASRTU", "备注名已存在，请使用其他名称。"));
				return;
			}
		}
		parsed.name = chosenName;
		parsed.overrideKey = program->overrideKey;
		parsed.customIndex = program->customIndex;
		parsed.sdrSharp = program->sdrSharp;
		QSettings settings;
		if (!parsed.overrideKey.isEmpty()) {
			settings.beginGroup(
			    QStringLiteral("sdr/programOverrides/") +
			    parsed.overrideKey);
			settings.setValue(QStringLiteral("name"), parsed.name);
			settings.setValue(QStringLiteral("executable"),
					  parsed.executable);
			settings.setValue(QStringLiteral("arguments"),
					  parsed.arguments);
			settings.endGroup();
		} else if (parsed.customIndex >= 0) {
			settings.beginWriteArray(
			    QStringLiteral("sdr/customPrograms"));
			settings.setArrayIndex(parsed.customIndex);
			settings.setValue(QStringLiteral("name"), parsed.name);
			settings.setValue(QStringLiteral("executable"),
					  parsed.executable);
			settings.setValue(QStringLiteral("arguments"),
					  parsed.arguments);
			settings.endArray();
		} else {
			const int count = settings.beginReadArray(
			    QStringLiteral("sdr/customPrograms"));
			settings.endArray();
			parsed.customIndex = count;
			settings.beginWriteArray(
			    QStringLiteral("sdr/customPrograms"), count + 1);
			settings.setArrayIndex(count);
			settings.setValue(QStringLiteral("name"), parsed.name);
			settings.setValue(QStringLiteral("executable"),
					  parsed.executable);
			settings.setValue(QStringLiteral("arguments"),
					  parsed.arguments);
			settings.endArray();
		}
		settings.sync();
		if (settings.status() != QSettings::NoError) {
			error->setText(
			    QCoreApplication::translate("ASRTU", "保存失败"));
			return;
		}
		*program = parsed;
		dialog.accept();
	});
	return dialog.exec() == QDialog::Accepted;
}

void showSdrChooser(QWidget *parent)
{
	QDialog dialog(parent);
	dialog.setObjectName(QStringLiteral("sdrChooser"));
	dialog.setWindowTitle(
	    QCoreApplication::translate("ASRTU", "选择 SDR 接收软件"));
	dialog.setWindowFlag(Qt::WindowContextHelpButtonHint, false);
	dialog.setMinimumWidth(680);
	dialog.setStyleSheet(QStringLiteral(
	    "QDialog#sdrChooser { background:#f7f9fc; }"
	    "QLabel { background:transparent; border:0; }"
	    "QLabel#sdrChooserTitle { color:#132238; font-size:17pt; "
	    "font-weight:600; }"
	    "QLabel#sdrChooserHint { color:#66758a; font-size:10pt; }"
	    "QScrollArea#sdrChooserScroll { background:transparent; border:0; }"
	    "QWidget#sdrChooserContent { background:transparent; }"
	    "QFrame#sdrCard { background:#ffffff; border:1px solid #dbe3ee; "
	    "border-radius:12px; }"
	    "QFrame#sdrCard:hover { border:1px solid #5b9be6; "
	    "background:#fbfdff; }"
	    // Override the parent's generic QPushButton min-height too: Qt's
	    // stylesheet polish otherwise resets setFixedHeight's minimum to 34
	    // and compresses the custom child layout down to one-pixel labels.
	    "QPushButton#sdrLaunch { min-height:128px; border:0; "
	    "background:transparent; padding:0; }"
	    "QPushButton#sdrLaunch:focus { border:1px solid #458bdd; "
	    "border-radius:8px; }"
	    "QLabel#sdrName { color:#172b4d; font-size:15pt; font-weight:600; }"
	    "QLabel#sdrDescription { color:#66758a; font-size:9pt; }"
	    "QLabel#sdrStatus { color:#187655; background:#edf8f1; "
	    "border-radius:5px; padding:3px 7px; font-size:9pt; }"
	    "QLabel#sdrStatus[unavailable=true] { color:#66758a; "
	    "background:#f0f2f5; }"
	    "QLabel#sdrArrow { color:#3478c6; font-size:15pt; }"
	    "QPushButton#sdrDelete { border:0; color:#9aa7b8; font-size:17px; "
	    "padding:8px; }"
	    "QPushButton#sdrDelete:hover { color:#d92d20; background:#fff1f0; "
	    "border-radius:7px; }"
	    "QPushButton#addSdrButton { min-height:32px; border:1px solid "
	    "#b9d2ee; padding:4px 14px; "
	    "border-radius:7px; background:#ffffff; color:#1769aa; "
	    "font-size:10pt; }"
	    "QPushButton#addSdrButton:hover { background:#eef7ff; "
	    "border-color:#5b9be6; }"
	    "QDialogButtonBox QPushButton { min-width:84px; min-height:32px; "
	    "border:1px solid #c9d5e3; "
	    "border-radius:7px; background:#ffffff; color:#344054; padding:4px "
	    "14px; }"
	    "QDialogButtonBox QPushButton:hover { background:#f0f6fc; }"));
	auto *layout = new QVBoxLayout(&dialog);
	layout->setContentsMargins(24, 22, 24, 20);
	layout->setSpacing(14);
	auto *title = new QLabel(
	    QCoreApplication::translate("ASRTU", "选择 SDR 接收软件"), &dialog);
	title->setObjectName(QStringLiteral("sdrChooserTitle"));
	layout->addWidget(title);
	auto *hint =
	    new QLabel(QCoreApplication::translate(
			   "ASRTU", "选择接收软件，开始卫星信号接收。"),
		       &dialog);
	hint->setObjectName(QStringLiteral("sdrChooserHint"));
	hint->setWordWrap(true);
	layout->addWidget(hint);
	auto *scroll = new QScrollArea(&dialog);
	scroll->setObjectName(QStringLiteral("sdrChooserScroll"));
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	auto *content = new QWidget(scroll);
	content->setObjectName(QStringLiteral("sdrChooserContent"));
	auto *grid = new QGridLayout(content);
	grid->setContentsMargins(0, 0, 0, 0);
	grid->setSpacing(12);
	grid->setAlignment(Qt::AlignTop);
	grid->setColumnStretch(0, 1);
	grid->setColumnStretch(1, 1);
	scroll->setWidget(content);
	scroll->viewport()->setAutoFillBackground(false);
	scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	layout->addWidget(scroll, 1);
	int count = 0;
	bool anyAvailable = false;
	const auto resizeCards = [&] {
		scroll->setFixedHeight(qMin((count + 1) / 2, 3) * 158 - 12);
		dialog.adjustSize();
	};
	const auto addCard = [&](const SdrProgram &program) {
		auto *card = new QFrame(content);
		card->setObjectName(QStringLiteral("sdrCard"));
		auto *row = new QHBoxLayout(card);
		row->setContentsMargins(8, 8, 8, 8);
		row->setSpacing(2);
		auto *button = new QPushButton(card);
		button->setObjectName(QStringLiteral("sdrLaunch"));
		button->setAutoDefault(false);
		button->setAccessibleName(program.name);
		button->setFixedHeight(128);
		button->setSizePolicy(QSizePolicy::Expanding,
				      QSizePolicy::Fixed);
		const bool available =
		    sdrExecutableAvailable(program.executable);
		anyAvailable = anyAvailable || available;
		button->setEnabled(available);
		button->setCursor(available ? Qt::PointingHandCursor
					    : Qt::ArrowCursor);
		button->setToolTip(
		    QDir::toNativeSeparators(program.executable));
		auto *body = new QVBoxLayout(button);
		body->setContentsMargins(10, 8, 10, 8);
		body->setSpacing(8);
		auto *heading = new QHBoxLayout;
		auto *icon = new QLabel(button);
		icon->setPixmap(sdrProgramIcon(program).pixmap(32, 32));
		heading->addWidget(icon);
		auto *name = new QLabel(program.name, button);
		name->setTextFormat(Qt::PlainText);
		name->setObjectName(QStringLiteral("sdrName"));
		heading->addWidget(name, 1);
		auto *arrow = new QLabel(
		    available ? QStringLiteral("→") : QString(), button);
		arrow->setObjectName(QStringLiteral("sdrArrow"));
		heading->addWidget(arrow);
		body->addLayout(heading);
		auto *description = new QLabel(
		    program.sdrSharp ? QCoreApplication::translate(
					   "ASRTU", "遥测预设 · 共享内存 I/Q")
		    : program.name == QStringLiteral("SDR++")
#ifdef Q_OS_WIN
			? QCoreApplication::translate(
			      "ASRTU", "遥测预设 · 共享内存 I/Q 与多普勒")
#else
			? QCoreApplication::translate("ASRTU",
						      "遥测预设 · Rigctl")
#endif
			: QFileInfo(program.executable).fileName(),
		    button);
		description->setTextFormat(Qt::PlainText);
		description->setObjectName(QStringLiteral("sdrDescription"));
		description->setWordWrap(true);
		body->addWidget(description);
		auto *status =
		    new QLabel(available ? QCoreApplication::translate(
					       "ASRTU", "就绪 · 点击打开")
					 : QCoreApplication::translate(
					       "ASRTU", "未安装或文件不完整"),
			       button);
		status->setObjectName(QStringLiteral("sdrStatus"));
		status->setProperty("unavailable", !available);
		body->addWidget(status, 0, Qt::AlignLeft);
		for (auto *label : button->findChildren<QLabel *>())
			label->setAttribute(Qt::WA_TransparentForMouseEvents);
		row->addWidget(button, 1);
		auto *edit = new QPushButton(card);
		edit->setObjectName(QStringLiteral("sdrEdit"));
		edit->setIcon(QIcon(QStringLiteral(":/launcher/edit.png")));
		edit->setIconSize(QSize(16, 16));
		edit->setToolTip(
		    QCoreApplication::translate("ASRTU", "修改此软件"));
		edit->setAccessibleName(edit->toolTip() + QStringLiteral(" ") +
					program.name);
		edit->setAutoDefault(false);
		edit->setFixedSize(28, 28);
		row->addWidget(edit, 0, Qt::AlignTop);
		QObject::connect(
		    edit, &QPushButton::clicked, &dialog, [&, program] {
			    SdrProgram updated = program;
			    if (!editSdrProgramDialog(&dialog, &updated, false))
				    return;
			    dialog.accept();
			    QTimer::singleShot(0, parent, [parent] {
				    showSdrChooser(parent);
			    });
		    });
		if (!program.sdrSharp &&
		    program.overrideKey != QStringLiteral("sdrpp")) {
			auto *remove =
			    new QPushButton(QStringLiteral("×"), card);
			remove->setObjectName(QStringLiteral("sdrDelete"));
			remove->setToolTip(
			    QCoreApplication::translate("ASRTU", "删除此软件"));
			remove->setAccessibleName(remove->toolTip() +
						  QStringLiteral(" ") +
						  program.name);
			remove->setAutoDefault(false);
			row->addWidget(remove, 0, Qt::AlignTop);
			QObject::connect(
			    remove, &QPushButton::clicked, &dialog,
			    [&, program, card] {
				    QSettings settings;
				    const int oldCount =
					settings.beginReadArray(QStringLiteral(
					    "sdr/customPrograms"));
				    QList<SdrProgram> retained;
				    for (int i = 0; i < oldCount; ++i) {
					    settings.setArrayIndex(i);
					    const QString name =
						settings
						    .value(
							QStringLiteral("name"))
						    .toString();
					    if (name.compare(
						    program.name,
						    Qt::CaseInsensitive) != 0)
						    retained.append(
							{name,
							 settings
							     .value(
								 QStringLiteral(
								     "executabl"
								     "e"))
							     .toString(),
							 settings
							     .value(
								 QStringLiteral(
								     "argument"
								     "s"))
							     .toStringList(),
							 {},
							 false,
							 {},
							 -1});
				    }
				    settings.endArray();
				    settings.remove(
					QStringLiteral("sdr/customPrograms"));
				    settings.beginWriteArray(
					QStringLiteral("sdr/customPrograms"),
					retained.size());
				    for (int i = 0; i < retained.size(); ++i) {
					    settings.setArrayIndex(i);
					    settings.setValue(
						QStringLiteral("name"),
						retained.at(i).name);
					    settings.setValue(
						QStringLiteral("executable"),
						retained.at(i).executable);
					    settings.setValue(
						QStringLiteral("arguments"),
						retained.at(i).arguments);
				    }
				    settings.endArray();
				    settings.sync();
				    if (settings.status() !=
					QSettings::NoError) {
					    QMessageBox::warning(
						&dialog,
						QCoreApplication::translate(
						    "ASRTU", "保存失败"),
						QCoreApplication::translate(
						    "ASRTU", "保存失败"));
					    return;
				    }
				    // Rebuild the chooser from QSettings.
				    // Keeping the remaining cards alive would
				    // preserve stale custom-array indexes after
				    // deletion and make a later edit write a
				    // duplicate entry.
				    Q_UNUSED(card);
				    dialog.accept();
				    QTimer::singleShot(0, parent, [parent] {
					    showSdrChooser(parent);
				    });
			    });
		}
		grid->addWidget(card, count / 2, count % 2);
		++count;
		QObject::connect(
		    button, &QPushButton::clicked, &dialog, [&, program] {
			    QString error;
			    qint64 processId = 0;
			    if (!startSdrProgram(program, &processId, &error)) {
				    QMessageBox::critical(
					&dialog,
					QCoreApplication::translate(
					    "ASRTU", "SDR 启动失败"),
					error);
				    return;
			    }
			    dialog.accept();
		    });
	};
	for (const auto &program : availableSdrPrograms())
		addCard(program);
	if (!anyAvailable)
		hint->setText(QCoreApplication::translate(
		    "ASRTU", "没有可用的 SDR 接收软件。请点击 + 添加。"));
	auto *add = new QPushButton(
	    QStringLiteral("＋  ") +
		QCoreApplication::translate("ASRTU", "添加自定义软件"),
	    &dialog);
	add->setObjectName(QStringLiteral("addSdrButton"));
	add->setAccessibleName(
	    QCoreApplication::translate("ASRTU", "添加 SDR 接收软件"));
	add->setToolTip(add->accessibleName());
	auto *footer = new QHBoxLayout;
	footer->addWidget(add);
	footer->addStretch();
	layout->addLayout(footer);
	QObject::connect(add, &QPushButton::clicked, &dialog, [&] {
		SdrProgram program;
		if (editSdrProgramDialog(&dialog, &program, true)) {
			addCard(program);
			resizeCards();
			hint->setText(QCoreApplication::translate(
			    "ASRTU", "选择接收软件，开始卫星信号接收。"));
		}
	});
	auto *close = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
	close->button(QDialogButtonBox::Close)
	    ->setText(QCoreApplication::translate("ASRTU", "关闭"));
	QObject::connect(close, &QDialogButtonBox::rejected, &dialog,
			 &QDialog::reject);
	footer->addWidget(close);
	resizeCards();
	dialog.exec();
#ifdef Q_OS_WIN
	// A path edited in the chooser can add or remove the bridge. Refresh
	// the shared-memory input item without requiring a launcher restart.
	if (parent) {
		if (auto *combo = parent->findChild<QComboBox *>(
			QStringLiteral("inputMode"))) {
			if (auto *model = qobject_cast<QStandardItemModel *>(
				combo->model()))
				model->item(0)->setEnabled(
				    sdrSharpBridgeAvailable() ||
				    sdrppBridgeAvailable());
		}
	}
#endif
}

bool startDoppler(double longitude, double latitude, double altitude,
		  const QString &satellite, qint64 *processId, QString *error)
{
	const QString name = executableName(QStringLiteral("TinyDoppler"));
	const QString directory =
	    asrtu::moduleDirectory(QStringLiteral("doppler"), name);
	const QString executable = QDir(directory).filePath(name);
	if (!QFileInfo::exists(executable)) {
		*error = QCoreApplication::translate(
		    "ASRTU", "未找到新版卫星跟踪程序 TinyDoppler.exe");
		return false;
	}
	QStringList arguments{QStringLiteral("--integrated"),
			      QStringLiteral("--longitude"),
			      QString::number(longitude, 'f', 6),
			      QStringLiteral("--latitude"),
			      QString::number(latitude, 'f', 6),
			      QStringLiteral("--altitude"),
			      QString::number(altitude, 'f', 2),
			      QStringLiteral("--satellite"),
			      satellite};
	const QString language = explicitLanguageArgument();
	if (!language.isEmpty())
		arguments.append(language);
	return startProgram(executable, arguments, directory, {}, false,
			    processId, error);
}

QString logTail(const QString &path)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		return {};
	if (file.size() > 4096)
		file.seek(file.size() - 4096);
	return QString::fromLocal8Bit(file.readAll()).trimmed();
}

void appendProxyLaunchLog(const QString &path, const QString &message)
{
	QFile file(path);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Append |
		       QIODevice::Text))
		return;
	QTextStream stream(&file);
	stream.setCodec("UTF-8");
	stream << QDateTime::currentDateTime().toString(
		      QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz "))
	       << message << '\n';
}

bool processSurvivedStartup(qint64 processId, int timeoutMs, quint32 *exitCode)
{
#ifdef Q_OS_WIN
	const HANDLE process =
	    OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
			DWORD(processId));
	if (!process)
		return false;
	const DWORD waitResult = WaitForSingleObject(process, DWORD(timeoutMs));
	DWORD nativeExitCode = STILL_ACTIVE;
	GetExitCodeProcess(process, &nativeExitCode);
	CloseHandle(process);
	if (exitCode)
		*exitCode = quint32(nativeExitCode);
	return waitResult == WAIT_TIMEOUT;
#else
	if (timeoutMs <= 0) {
		const bool running =
		    processId > 0 &&
		    (::kill(static_cast<pid_t>(processId), 0) == 0 ||
		     errno != ESRCH);
		if (exitCode)
			*exitCode = running ? 0U : 1U;
		return running;
	}
	QElapsedTimer timer;
	timer.start();
	while (timer.elapsed() < timeoutMs) {
		if (processId <= 0 ||
		    (::kill(static_cast<pid_t>(processId), 0) != 0 &&
		     errno == ESRCH)) {
			if (exitCode)
				*exitCode = 1;
			return false;
		}
		QThread::msleep(25);
	}
	if (exitCode)
		*exitCode = 0;
	return true;
#endif
}

void terminateDetachedProcess(qint64 processId)
{
	if (processId <= 0)
		return;
#ifdef Q_OS_WIN
	HANDLE process = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE,
				     DWORD(processId));
	if (!process)
		return;
	TerminateProcess(process, ERROR_CANCELLED);
	WaitForSingleObject(process, 2000);
	CloseHandle(process);
#else
	const pid_t pid = static_cast<pid_t>(processId);
	if (::kill(pid, SIGTERM) != 0 && errno == ESRCH)
		return;
	for (int attempt = 0; attempt < 40; ++attempt) {
		if (::kill(pid, 0) != 0 && errno == ESRCH)
			return;
		QThread::msleep(25);
	}
	::kill(pid, SIGKILL);
#endif
}

bool startSatnogsUploader(const QString &satellite, int noradId,
			  const QString &source, double longitude,
			  double latitude, double altitude, qint64 *processId,
			  QString *error, bool *alreadyRunning = nullptr)
{
	if (alreadyRunning)
		*alreadyRunning = false;
	const QString directory = asrtu::moduleDirectory(
	    QStringLiteral("satnogs-uploader"),
	    executableName(QStringLiteral("ASRTU_SatnogsUploader")));
	const QString uploader = QDir(directory).filePath(
	    executableName(QStringLiteral("ASRTU_SatnogsUploader")));
	QStringList arguments{
	    QStringLiteral("--satellite"), satellite,
	    QStringLiteral("--norad-id"),  QString::number(noradId),
	    QStringLiteral("--source"),	   source,
	    QStringLiteral("--longitude"), QString::number(longitude, 'f', 6),
	    QStringLiteral("--latitude"),  QString::number(latitude, 'f', 6),
	    QStringLiteral("--altitude"),  QString::number(altitude, 'f', 2)};
	const QString language = explicitLanguageArgument();
	if (!language.isEmpty())
		arguments.append(language);
	qint64 uploaderPid = 0;
	if (!startProgram(uploader, arguments, directory, {}, false,
			  &uploaderPid, error)) {
		return false;
	}
	quint32 exitCode = 0;
	if (!processSurvivedStartup(uploaderPid, 1200, &exitCode)) {
		if (exitCode == 3) {
			if (alreadyRunning)
				*alreadyRunning = true;
			if (processId)
				*processId = 0;
			return true;
		}
		*error =
		    QCoreApplication::translate(
			"ASRTU", "SatNOGS 上传程序启动后立即退出（代码 %1）。")
			.arg(exitCode);
		return false;
	}
	*processId = uploaderPid;
	return true;
}

bool startProxy(const QString &launchLog, qint64 *processId, QString *error,
		bool *alreadyRunning = nullptr)
{
	if (alreadyRunning)
		*alreadyRunning = false;
#ifdef Q_OS_WIN
	const QString proxyWrapper =
	    proxyDirectory() + QStringLiteral("/ASRTU_Proxy.exe");
	const QString proxy =
	    QFileInfo::exists(proxyWrapper)
		? proxyWrapper
		: proxyDirectory() + QStringLiteral("/proxy_mmt_gui.exe");
#else
	const QString proxy =
	    QDir(QCoreApplication::applicationDirPath())
		.filePath(QStringLiteral("ASRTU_UploadProxy"));
#endif
	if (!QFileInfo::exists(configPath())) {
		*error = QCoreApplication::translate(
		    "ASRTU", "尚未配置上传信息，请先填写呼号和地面站资料。");
		return false;
	}
	if (!launchLog.isEmpty()) {
		appendProxyLaunchLog(
		    launchLog,
		    QStringLiteral("Starting interactive proxy console: %1")
			.arg(proxy));
	}
	qint64 proxyPid = 0;
	QStringList proxyArguments;
#ifndef Q_OS_WIN
	proxyArguments << QStringLiteral("--config") << configPath()
		       << QStringLiteral("--gui");
	const QString language = explicitLanguageArgument();
	if (!language.isEmpty())
		proxyArguments.append(language);
#endif
#ifdef Q_OS_WIN
	const QString proxyOutputLog;
	constexpr bool visibleProxyConsole = true;
#else
	const QString &proxyOutputLog = launchLog;
	constexpr bool visibleProxyConsole = false;
#endif
	if (!startProgram(proxy, proxyArguments, proxyDirectory(),
			  proxyOutputLog, visibleProxyConsole, &proxyPid,
			  error))
		return false;
	quint32 exitCode = 0;
	if (!processSurvivedStartup(proxyPid, 1200, &exitCode)) {
		if (exitCode == 3) {
			if (alreadyRunning)
				*alreadyRunning = true;
			*processId = 0;
			return true;
		}
		*error = QCoreApplication::translate(
			     "ASRTU", "上传代理启动后立即退出（代码 %1）。")
			     .arg(exitCode);
		if (!launchLog.isEmpty()) {
			const QString output = logTail(launchLog);
			*error +=
			    QCoreApplication::translate("ASRTU", "\n日志：%1")
				.arg(launchLog);
			if (!output.isEmpty())
				*error += QCoreApplication::translate(
					      "ASRTU", "\n\n最后输出：\n%1")
					      .arg(output);
		}
		return false;
	}
	if (!launchLog.isEmpty()) {
		appendProxyLaunchLog(
		    launchLog,
		    QStringLiteral("Proxy startup check passed; PID=%1")
			.arg(proxyPid));
	}
	*processId = proxyPid;
	return true;
}

bool startSuite(bool enableProxy, int inputMode, const QString &wavPath,
		bool fastPlayback, int audioDeviceId, bool recordingEnabled,
		QString *sessionDirectory, qint64 *proxyProcessId,
		QString *error)
{
	inputMode = normalizedInputMode(inputMode);
	const QString decoder =
	    QDir(decoderDirectory())
		.filePath(executableName(QStringLiteral("ASRTU1_Demod_CQt")));
	*sessionDirectory = createSessionDirectory(error);
	if (sessionDirectory->isEmpty())
		return false;
	QString playbackPath = wavPath;
	if (!playbackPath.isEmpty()) {
		if (QFileInfo(playbackPath)
			.suffix()
			.compare(QStringLiteral("wav"), Qt::CaseInsensitive) !=
		    0) {
			const QString converter = audioConverterPath();
			if (converter.isEmpty()) {
				*error = QCoreApplication::translate(
				    "ASRTU", "当前安装没有录音格式转换器，仅能"
					     "播放 WAV 录音。");
				return false;
			}
			const QString converted =
			    QDir(*sessionDirectory)
				.filePath(QStringLiteral("playback_input.wav"));
			QProcess conversion;
			conversion.setProgram(converter);
			conversion.setArguments({QStringLiteral("-pcm16"),
						 playbackPath, converted});
			conversion.setWorkingDirectory(decoderDirectory());
			conversion.start();
			if (!conversion.waitForStarted(3000) ||
			    !conversion.waitForFinished(60000) ||
			    conversion.exitStatus() != QProcess::NormalExit ||
			    conversion.exitCode() != 0) {
				*error =
				    QCoreApplication::translate(
					"ASRTU", "无法转换录音文件：/%1")
					.arg(
					    QFileInfo(playbackPath).fileName());
				return false;
			}
			playbackPath = converted;
		}
		const int channels = wavChannelCount(playbackPath);
		if (channels != 1 && channels != 2) {
			*error = QCoreApplication::translate(
			    "ASRTU", "仅支持单声道或双声道录音文件。");
			return false;
		}
		inputMode = channels == 1 ? 1 : 0;
	}
	qint64 proxyPid = 0;
	if (enableProxy) {
		const QString proxyLog =
		    QDir(*sessionDirectory)
			.filePath(QStringLiteral("proxy.log"));
		bool proxyAlreadyRunning = false;
		if (!startProxy(proxyLog, &proxyPid, error,
				&proxyAlreadyRunning)) {
			// The receiver can still decode and record without the
			// legacy MMT upload proxy. Do not let a dead proxy
			// block the whole session.
			const QString proxyError = *error;
			appendProxyLaunchLog(
			    proxyLog,
			    QStringLiteral("MMT upload proxy unavailable: %1")
				.arg(proxyError));
			error->clear();
			proxyPid = 0;
		} else if (proxyAlreadyRunning) {
			appendProxyLaunchLog(
			    proxyLog, QStringLiteral("MMT upload proxy already "
						     "running; reusing it."));
			proxyPid = 0;
		}
	}
	qint64 decoderPid = 0;
	// A file handshake works across restricted/low-integrity launch
	// contexts where QLocalSocket named-pipe connections can be denied.
	const QString readyFile =
	    QDir(*sessionDirectory).filePath(QStringLiteral("decoder.ready"));
	QFile::remove(readyFile);
	QStringList decoderArguments{QStringLiteral("--session-dir"),
				     *sessionDirectory,
				     QStringLiteral("--ready-file"), readyFile};
	if (!playbackPath.isEmpty())
		decoderArguments << QStringLiteral("--wav") << playbackPath;
	if (!playbackPath.isEmpty() && fastPlayback)
		decoderArguments << QStringLiteral("--fast-playback");
	else if (!recordingEnabled)
		decoderArguments << QStringLiteral("--no-record");
	if (inputMode == 1)
		decoderArguments << QStringLiteral("--real-if-12k");
	else if (inputMode == 2)
		decoderArguments << QStringLiteral("--shared-iq-bridge");
	if (playbackPath.isEmpty() && inputMode != 2)
		decoderArguments << QStringLiteral("--audio-device")
				 << QString::number(audioDeviceId);
	const QString decoderLog =
	    QDir(*sessionDirectory).filePath(QStringLiteral("decoder.log"));
	if (!startProgram(decoder, decoderArguments, decoderDirectory(),
			  decoderLog, false, &decoderPid, error)) {
		if (proxyPid > 0)
			terminateDetachedProcess(proxyPid);
		return false;
	}
	QElapsedTimer readyTimer;
	readyTimer.start();
	bool decoderReady = false;
	quint32 decoderExitCode = 0;
	while (readyTimer.elapsed() < 15000) {
		QFile ready(readyFile);
		if (ready.open(QIODevice::ReadOnly) &&
		    ready.readAll().trimmed() == QByteArrayLiteral("ready")) {
			decoderReady = true;
			break;
		}
		if (!processSurvivedStartup(decoderPid, 0, &decoderExitCode))
			break;
		QThread::msleep(25);
	}
	if (!decoderReady) {
		terminateDetachedProcess(decoderPid);
		if (proxyPid > 0)
			terminateDetachedProcess(proxyPid);
		*error =
		    decoderExitCode == 3
			? QCoreApplication::translate(
			      "ASRTU", "实时解码器已在运行；不会重复启动。")
			: QCoreApplication::translate(
			      "ASRTU",
			      "解码器未在 15 秒内完成初始化；已回滚本次启动。");
		return false;
	}
	QFile::remove(readyFile);
	*proxyProcessId = proxyPid;
	return true;
}

class LauncherWindow final : public QWidget
{
      public:
	LauncherWindow()
	{
		// Keep the launcher available while decoder/proxy windows are
		// created.
		setWindowIcon(QIcon(
		    QStringLiteral(":/launcher/astro_series_launcher.png")));
		setWindowTitle(QCoreApplication::translate(
				   "ASRTU", "阿斯图系列卫星启动器") +
			       QStringLiteral(" v") +
			       QStringLiteral(ASRTU_VERSION));
		// Keep the launcher compact on low-resolution and high-DPI
		// displays. The scroll area below keeps all controls reachable
		// at the minimum size.
		setMinimumSize(260, 300);
		setStyleSheet(QStringLiteral(
		    "QWidget#launcherRoot { background:#f3f6f9; color:#17202a; "
		    "}"
		    "QLabel#subtitle { color:#667085; }"
		    "QFrame#card { background:#ffffff; border:1px solid "
		    "#dce3ea; "
		    "border-radius:8px; }"
		    "QLabel#sectionTitle { color:#344054; font-weight:600; }"
		    "QLineEdit,QDoubleSpinBox,QComboBox { min-height:30px; "
		    "padding:0 9px; "
		    "background:#ffffff; color:#17202a; border:1px solid "
		    "#cbd5e1; "
		    "border-radius:5px; selection-background-color:#dceeff; "
		    "selection-color:#17202a; }"
		    "QComboBox QAbstractItemView { background:#ffffff; "
		    "color:#17202a; "
		    "selection-background-color:#dceeff; "
		    "selection-color:#17202a; "
		    "outline:0; }"
		    "QComboBox QAbstractItemView::item:hover { "
		    "background:#dceeff; "
		    "color:#17202a; }"
		    "QLineEdit:focus,QDoubleSpinBox:focus,QComboBox:focus { "
		    "border:1px solid #2b7de9; }"
		    "QComboBox::drop-down { border:0; width:25px; }"
		    "QComboBox::down-arrow { "
		    "image:url(:/launcher/combo_arrow.png); "
		    "width:16px; height:16px; }"
		    "QPushButton { min-height:34px; padding:0 16px; "
		    "color:#344054; "
		    "background:#ffffff; border:1px solid #cbd5e1; "
		    "border-radius:6px; }"
		    "QPushButton:hover { background:#f7f9fc; "
		    "border-color:#98a6b5; }"
		    "QPushButton:pressed { background:#edf1f5; }"
		    "QPushButton#tool { min-height:38px; background:#f8fafc; "
		    "border-color:#d8e0e8; font-weight:500; }"
		    "QPushButton#tool:hover { background:#eef5ff; "
		    "border-color:#80aee8; "
		    "color:#145ca8; }"
		    "QPushButton#primary { min-height:38px; color:#ffffff; "
		    "background:#16834f; border:1px solid #16834f; "
		    "font-weight:600; }"
		    "QPushButton#primary:hover { background:#116f42; "
		    "border-color:#116f42; }"
		    "QPushButton#primary:pressed { background:#0d5d37; }"
		    "QPushButton#secondaryAction { min-height:38px; "
		    "color:#145ca8; "
		    "background:#edf5ff; border:1px solid #a9c9ef; "
		    "font-weight:600; }"
		    "QPushButton#secondaryAction:hover { background:#dfeeff; "
		    "border-color:#7eafe8; }"
		    "QScrollBar:vertical { background:transparent; width:10px; "
		    "margin:2px; }"
		    "QScrollBar::handle:vertical { background:#b8c6d6; "
		    "border-radius:4px; "
		    "min-height:32px; }"
		    "QScrollBar::handle:vertical:hover { background:#91a4b8; }"
		    "QScrollBar::add-line:vertical,QScrollBar::sub-line:"
		    "vertical { height:0; }"
		    "QScrollBar::add-page:vertical,QScrollBar::sub-page:"
		    "vertical { "
		    "background:transparent; }"));
		setObjectName(QStringLiteral("launcherRoot"));

		auto *windowLayout = new QVBoxLayout(this);
		windowLayout->setContentsMargins(0, 0, 0, 0);
		auto *scroll = new QScrollArea(this);
		scroll->setFrameShape(QFrame::NoFrame);
		scroll->setWidgetResizable(true);
		scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		auto *content = new QWidget(scroll);
		content->setObjectName(QStringLiteral("launcherRoot"));
		auto *layout = new QVBoxLayout(content);
		layout->setContentsMargins(24, 16, 24, 12);
		layout->setSpacing(9);
		scroll->setWidget(content);
		windowLayout->addWidget(scroll);
		auto *title =
		    new QLabel(QCoreApplication::translate(
				   "ASRTU", "阿斯图系列卫星接收与遥测上传"),
			       this);
		QFont titleFont = title->font();
		titleFont.setPointSize(14);
		titleFont.setBold(true);
		title->setFont(titleFont);
		layout->addWidget(title);
		auto *subtitle = new QLabel(
		    QCoreApplication::translate(
			"ASRTU",
			"配置接收输入与地面站资料，再按需启动各个组件。"),
		    this);
		subtitle->setObjectName(QStringLiteral("subtitle"));
		layout->addWidget(subtitle);

		auto *configurationCard = new QFrame(this);
		configurationCard->setObjectName(QStringLiteral("card"));
		configurationCard->setSizePolicy(QSizePolicy::Preferred,
						 QSizePolicy::Minimum);
		auto *configurationLayout = new QVBoxLayout(configurationCard);
		configurationLayout->setSizeConstraint(QLayout::SetMinimumSize);
		configurationLayout->setContentsMargins(18, 11, 18, 12);
		configurationLayout->setSpacing(7);
		auto *configurationTitle = new QLabel(
		    QCoreApplication::translate("ASRTU", "接收与上传设置"),
		    configurationCard);
		configurationTitle->setObjectName(
		    QStringLiteral("sectionTitle"));
		configurationLayout->addWidget(configurationTitle);
		form_ = new QFormLayout;
		auto *form = form_;
		form->setContentsMargins(0, 0, 0, 0);
		form->setHorizontalSpacing(16);
		form->setVerticalSpacing(6);
		form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
		form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
		nickname_ = new QLineEdit(this);
		nickname_->setMaxLength(32);
		longitude_ = makeCoordinate(-180.0, 180.0, 6);
		latitude_ = makeCoordinate(-90.0, 90.0, 6);
		altitude_ = makeCoordinate(-500.0, 10000.0, 2);
		altitude_->setSuffix(QStringLiteral(" m"));
		input_mode_ = new QComboBox(this);
		input_mode_->setObjectName(QStringLiteral("inputMode"));
		input_mode_->setSizeAdjustPolicy(
		    QComboBox::AdjustToMinimumContentsLengthWithIcon);
		input_mode_->setMinimumContentsLength(18);
#ifdef Q_OS_WIN
		const bool localBridgeAvailable =
		    sdrSharpBridgeAvailable() || sdrppBridgeAvailable();
		const QString sharedBridgeLabel = QCoreApplication::translate(
		    "ASRTU", "本地内存共享 RAW 模式 I/Q 桥接");
		input_mode_->addItem(sharedBridgeLabel, 2);
		if (!localBridgeAvailable)
			if (auto *model = qobject_cast<QStandardItemModel *>(
				input_mode_->model()))
				model->item(0)->setEnabled(false);
#else
		input_mode_->addItem(
		    QCoreApplication::translate(
			"ASRTU", "本地内存共享 RAW 模式 I/Q 桥接"),
		    2);
#endif
		input_mode_->addItem(
		    QCoreApplication::translate(
			"ASRTU", "立体声零中频 RAW 模式 I/Q 输入"),
		    0);
		input_mode_->addItem(
		    QCoreApplication::translate(
			"ASRTU", "单声道实数域 12KHz 电台 IF 输入"),
		    1);
		audio_device_ = new QComboBox(this);
		audio_device_->setSizeAdjustPolicy(
		    QComboBox::AdjustToMinimumContentsLengthWithIcon);
		audio_device_->setMinimumContentsLength(18);
		refreshAudioDevices();
		recording_enabled_ =
		    new QCheckBox(QCoreApplication::translate(
				      "ASRTU", "自动保存本次接收录音"),
				  this);
		recording_enabled_->setChecked(false);
		for (QWidget *field : {static_cast<QWidget *>(input_mode_),
				       static_cast<QWidget *>(audio_device_),
				       static_cast<QWidget *>(nickname_),
				       static_cast<QWidget *>(longitude_),
				       static_cast<QWidget *>(latitude_),
				       static_cast<QWidget *>(altitude_)}) {
			// QSS min-height alone is not included reliably in
			// QFormLayout's minimum size at fractional Windows DPI
			// scaling.
			field->setMinimumHeight(30);
			field->setSizePolicy(QSizePolicy::Expanding,
					     QSizePolicy::Fixed);
		}
		form->addRow(QCoreApplication::translate("ASRTU", "输入"),
			     input_mode_);
		form->addRow(QCoreApplication::translate("ASRTU", "声卡"),
			     audio_device_);
		form->addRow(QCoreApplication::translate("ASRTU", "呼号"),
			     nickname_);
		form->addRow(QCoreApplication::translate("ASRTU", "经度"),
			     longitude_);
		form->addRow(QCoreApplication::translate("ASRTU", "纬度"),
			     latitude_);
		form->addRow(QCoreApplication::translate("ASRTU", "海拔"),
			     altitude_);
		configurationLayout->addLayout(form);

		audio_device_label_ = form->labelForField(audio_device_);
		connect(input_mode_,
			qOverload<int>(&QComboBox::currentIndexChanged), this,
			[this](int) { updateAudioDeviceVisibility(); });
		auto *audioRefreshTimer = new QTimer(this);
		connect(audioRefreshTimer, &QTimer::timeout, this, [this] {
			if (input_mode_->currentData().toInt() != 2)
				refreshAudioDevices();
		});
		audioRefreshTimer->start(2000);

		layout->addWidget(configurationCard);

		auto *toolsCard = new QFrame(this);
		toolsCard->setObjectName(QStringLiteral("card"));
		auto *toolsLayout = new QVBoxLayout(toolsCard);
		toolsLayout->setContentsMargins(18, 10, 18, 11);
		toolsLayout->setSpacing(6);
		auto *toolsTitle =
		    new QLabel(QCoreApplication::translate("ASRTU", "常用工具"),
			       toolsCard);
		toolsTitle->setObjectName(QStringLiteral("sectionTitle"));
		toolsLayout->addWidget(toolsTitle);
		auto *tools = new QGridLayout;
		tools->setHorizontalSpacing(9);
		auto *sdrButton =
		    new QPushButton(QCoreApplication::translate(
					"ASRTU", "打开预设 SDR 接收软件"),
				    this);
		auto *tracker =
		    new QPushButton(QCoreApplication::translate(
					"ASRTU", "卫星跟踪与自动多普勒"),
				    this);
		auto *playback = new QPushButton(
		    QCoreApplication::translate("ASRTU", "播放录音文件"), this);
		auto *quickReplay = new QPushButton(
		    QCoreApplication::translate("ASRTU", "快速重放录音"), this);
		auto *openRecords = new QPushButton(
		    QCoreApplication::translate("ASRTU", "打开录音目录"), this);
		for (auto *button :
		     {sdrButton, tracker, playback, quickReplay, openRecords}) {
			button->setObjectName(QStringLiteral("tool"));
			// Long translated captions must not increase the
			// top-level window's minimum width; the two equal grid
			// columns provide ample room at the compact launcher
			// size.
			button->setSizePolicy(QSizePolicy::Expanding,
					      QSizePolicy::Fixed);
		}
		tools->addWidget(sdrButton, 0, 0);
		tools->addWidget(tracker, 0, 1);
		tools->addWidget(playback, 1, 0);
		tools->addWidget(quickReplay, 1, 1);
		tools->addWidget(openRecords, 2, 0, 1, 2);
		toolsLayout->addLayout(tools);
		layout->addWidget(toolsCard);
		status_label_ = new QLabel(
		    QCoreApplication::translate("ASRTU", "就绪"), this);
		status_label_->setStyleSheet(
		    QStringLiteral("color:#667788; padding:0 2px 2px 2px;"));
		// A long playback filename must not widen the launcher; the
		// label width is layout-driven and long text is elided in
		// setStatusText().
		status_label_->setMinimumWidth(0);
		status_label_->setSizePolicy(QSizePolicy::Expanding,
					     QSizePolicy::Preferred);
		status_label_->installEventFilter(this);
		layout->addWidget(status_label_);

		auto *buttons = new QGridLayout;
		buttons->setSpacing(9);
		auto *start = new QPushButton(
		    QCoreApplication::translate("ASRTU", "启动接收"), this);
		auto *startProxyButton = new QPushButton(
		    QCoreApplication::translate("ASRTU", "启动上传"), this);
		start->setObjectName(QStringLiteral("primary"));
		startProxyButton->setObjectName(
		    QStringLiteral("secondaryAction"));
		const int actionWidth =
		    std::max({start->sizeHint().width(),
			      startProxyButton->sizeHint().width(),
			      recording_enabled_->sizeHint().width()});
		start->setMinimumWidth(actionWidth);
		startProxyButton->setMinimumWidth(actionWidth);
		start->setSizePolicy(QSizePolicy::MinimumExpanding,
				     QSizePolicy::Fixed);
		startProxyButton->setSizePolicy(QSizePolicy::MinimumExpanding,
						QSizePolicy::Fixed);
		buttons->setColumnStretch(0, 1);
		buttons->setColumnMinimumWidth(1, actionWidth);
		buttons->setColumnMinimumWidth(2, actionWidth);
		buttons->addWidget(startProxyButton, 0, 1);
		buttons->addWidget(start, 0, 2);
		buttons->addWidget(recording_enabled_, 1, 2, Qt::AlignHCenter);
		layout->addLayout(buttons);

		auto *credit = new QLabel(QStringLiteral("By BG7ZDQ"), this);
		credit->setAlignment(Qt::AlignRight);
		credit->setStyleSheet(QStringLiteral(
		    "color:#7b8794; font-style:italic; padding-top:1px;"));
		layout->addWidget(credit);

		connect(sdrButton, &QPushButton::clicked, this,
			[this] { showSdrChooser(this); });
		tracker->setProperty("satelliteTracker", true);
		connect(tracker, &QPushButton::clicked, this, [this] {
			qint64 processId = 0;
			QString error;
			if (!startDoppler(longitude_->value(),
					  latitude_->value(),
					  altitude_->value(), savedSatellite(),
					  &processId, &error)) {
				QMessageBox::critical(
				    this,
				    QCoreApplication::translate(
					"ASRTU", "多普勒工具启动失败"),
				    error);
			}
		});
		connect(openRecords, &QPushButton::clicked, this, [this] {
			const QString directory = recordsDirectory();
			if (!QDir().mkpath(directory) ||
			    !QDesktopServices::openUrl(
				QUrl::fromLocalFile(directory))) {
				QMessageBox::critical(
				    this,
				    QCoreApplication::translate("ASRTU",
								"无法打开目录"),
				    directory);
			}
		});
		auto playRecording = [this](const QString &initialDirectory,
					    bool fastPlayback,
					    const QString &dialogTitle,
					    const QString &errorTitle) {
			QString recordingFilter = QCoreApplication::translate(
			    "ASRTU", "接收录音 (*.wav");
			if (!audioConverterPath().isEmpty())
				recordingFilter += QStringLiteral(
				    " *.ogg *.oga *.opus *.flac *.mp3");
			recordingFilter += QLatin1Char(')');
			const QString wavPath = QFileDialog::getOpenFileName(
			    this, dialogTitle, initialDirectory,
			    recordingFilter);
			if (wavPath.isEmpty())
				return;
			QString error;
			QString sessionDirectory;
			qint64 proxyProcessId = 0;
			if (!startSuite(false, 0, wavPath, fastPlayback, -1,
					false, &sessionDirectory,
					&proxyProcessId, &error)) {
				QMessageBox::critical(this, errorTitle, error);
				return;
			}
			setStatusText(QCoreApplication::translate(
					  "ASRTU", "正在播放：/%1；日志：%2")
					  .arg(QFileInfo(wavPath).fileName(),
					       compactSessionDirectory(
						   sessionDirectory)));
		};
		connect(playback, &QPushButton::clicked, this,
			[this, playRecording] {
				playRecording(QString(), false,
					      QCoreApplication::translate(
						  "ASRTU", "选择接收录音"),
					      QCoreApplication::translate(
						  "ASRTU", "播放失败"));
			});
		connect(quickReplay, &QPushButton::clicked, this,
			[this, playRecording] {
				QDir().mkpath(recordsDirectory());
				playRecording(recordsDirectory(), true,
					      QCoreApplication::translate(
						  "ASRTU", "快速重放录音"),
					      QCoreApplication::translate(
						  "ASRTU", "快速重放失败"));
			});
		connect(startProxyButton, &QPushButton::clicked, this, [this] {
			bool accepted = false;
			bool satnogsEnabled = false;
			const QString satellite =
			    chooseSatellite(&accepted, &satnogsEnabled);
			if (!accepted || !saveConfiguration(false, satellite))
				return;
			QSettings settings(
			    QStringLiteral("ASRTU"),
			    QStringLiteral("AstroSeriesLauncher"));
			settings.setValue(QStringLiteral("satnogs_enabled"),
					  satnogsEnabled);
			settings.setValue(QStringLiteral("satnogs_norad_id"),
					  noradForSatellite(satellite));
			settings.setValue(QStringLiteral("satnogs_source"),
					  nickname_->text().trimmed());
			settings.setValue(QStringLiteral("satnogs_longitude"),
					  longitude_->value());
			settings.setValue(QStringLiteral("satnogs_latitude"),
					  latitude_->value());
			settings.sync();
			qint64 satnogsProcessId = 0;
			bool satnogsAlreadyRunning = false;
			if (satnogsEnabled) {
				QString error;
				if (!startSatnogsUploader(
					satellite, noradForSatellite(satellite),
					nickname_->text().trimmed(),
					longitude_->value(), latitude_->value(),
					altitude_->value(), &satnogsProcessId,
					&error, &satnogsAlreadyRunning)) {
					QMessageBox::critical(
					    this,
					    QCoreApplication::translate(
						"ASRTU",
						"SatNOGS 上传程序启动失败"),
					    error);
					return;
				}
			}
			// Satellites without an MMT server use SatNOGS as their
			// mandatory and only upload path. MMT-backed satellites
			// may run both paths.
			if (webSocketForSatellite(satellite).isEmpty()) {
				setStatusText(
				    satnogsAlreadyRunning
					? QCoreApplication::translate(
					      "ASRTU",
					      "SatNOGS 上传程序已在运行：%1")
					      .arg(satellite)
					: QCoreApplication::translate(
					      "ASRTU",
					      "SatNOGS 上传程序已启动：%1（PID "
					      "%2）")
					      .arg(satellite)
					      .arg(satnogsProcessId));
				return;
			}
			QString error;
			qint64 processId = 0;
			bool proxyAlreadyRunning = false;
			if (!startProxy({}, &processId, &error,
					&proxyAlreadyRunning)) {
				if (satnogsProcessId > 0)
					terminateDetachedProcess(
					    satnogsProcessId);
				QMessageBox::critical(
				    this,
				    QCoreApplication::translate(
					"ASRTU", "上传代理启动失败"),
				    error);
				return;
			}
			if (proxyAlreadyRunning && !satnogsEnabled) {
				setStatusText(QCoreApplication::translate(
				    "ASRTU", "MMT 上传代理已在运行"));
			} else if (proxyAlreadyRunning &&
				   satnogsAlreadyRunning) {
				setStatusText(QCoreApplication::translate(
				    "ASRTU",
				    "MMT 与 SatNOGS 上传程序均已在运行"));
				QMessageBox::warning(
				    this,
				    QCoreApplication::translate(
					"ASRTU", "上传程序已在运行"),
				    QCoreApplication::translate(
					"ASRTU",
					"MMT 与 SatNOGS "
					"上传程序均已在运行；不会重复启动。"));
			} else if (proxyAlreadyRunning) {
				setStatusText(
				    QCoreApplication::translate(
					"ASRTU",
					"MMT 上传代理已在运行；SatNOGS "
					"上传程序已启动（PID %1）")
					.arg(satnogsProcessId));
			} else if (satnogsAlreadyRunning) {
				setStatusText(
				    QCoreApplication::translate(
					"ASRTU",
					"MMT 上传代理已启动（PID "
					"%1）；SatNOGS 上传程序已在运行")
					.arg(processId));
			} else {
				setStatusText(
				    satnogsEnabled
					? QCoreApplication::translate(
					      "ASRTU",
					      "MMT 与 SatNOGS "
					      "上传均已启动（PID %1 / %2）")
					      .arg(processId)
					      .arg(satnogsProcessId)
					: QCoreApplication::translate(
					      "ASRTU",
					      "上传代理已启动（PID %1）")
					      .arg(processId));
			}
		});
		connect(start, &QPushButton::clicked, this, [this] {
			if (!inputModeIndexEnabled(
				input_mode_->currentIndex())) {
				const int fallback =
				    firstEnabledInputModeIndex();
				input_mode_->setCurrentIndex(fallback);
				updateAudioDeviceVisibility();
				QMessageBox::warning(
				    this,
				    QCoreApplication::translate(
					"ASRTU", "输入模式不可用"),
				    QCoreApplication::translate(
					"ASRTU",
					"当前输入模式不可用，已切换到可用的输"
					"入模式；请确认后再次启动。"));
				return;
			}
			const int inputMode =
			    input_mode_->currentData().toInt();
			const int channelCount =
			    audio_device_->currentData(kAudioChannelCountRole)
				.toInt();
			if (inputMode == 0 && channelCount == 1) {
				QMessageBox::warning(
				    this,
				    QCoreApplication::translate(
					"ASRTU", "输入声道不匹配"),
				    QCoreApplication::translate(
					"ASRTU",
					"当前模式需要双声道 "
					"I/Q，但所选声卡仅提供单声道。\n"
					"若输入来自电台 12 kHz 中频或 SDR# 的 "
					"USB 音频，请改选“单声道实数域 12KHz "
					"电台 IF 输入”。"));
				return;
			}
			QSettings settings(
			    QStringLiteral("ASRTU"),
			    QStringLiteral("AstroSeriesLauncher"));
			settings.setValue(QStringLiteral("input_mode"),
					  input_mode_->currentData().toInt());
			settings.setValue(QStringLiteral("recording_enabled"),
					  recording_enabled_->isChecked());
			settings.setValue(QStringLiteral("audio_device_name"),
					  audio_device_->currentText());
			QString error;
			QString sessionDirectory;
			qint64 proxyProcessId = 0;
			if (!startSuite(false, inputMode, {}, false,
					audio_device_->currentData().toInt(),
					recording_enabled_->isChecked(),
					&sessionDirectory, &proxyProcessId,
					&error)) {
				QMessageBox::critical(
				    this,
				    QCoreApplication::translate("ASRTU",
								"启动失败"),
				    error);
				return;
			}
			setStatusText(QCoreApplication::translate(
					  "ASRTU", "接收已启动；录音与日志：%1")
					  .arg(compactSessionDirectory(
					      sessionDirectory)));
		});
		loadConfiguration();
		updateAudioDeviceVisibility();
		autoSaveTimer_ = new QTimer(this);
		autoSaveTimer_->setSingleShot(true);
		autoSaveTimer_->setInterval(350);
		connect(autoSaveTimer_, &QTimer::timeout, this, [this] {
			QSettings settings(
			    QStringLiteral("ASRTU"),
			    QStringLiteral("AstroSeriesLauncher"));
			settings.setValue(QStringLiteral("input_mode"),
					  input_mode_->currentData());
			settings.setValue(QStringLiteral("recording_enabled"),
					  recording_enabled_->isChecked());
			settings.setValue(QStringLiteral("audio_device_name"),
					  audio_device_->currentText());
			settings.setValue(QStringLiteral("nickname"),
					  nickname_->text().trimmed());
			settings.setValue(QStringLiteral("longitude"),
					  longitude_->value());
			settings.setValue(QStringLiteral("latitude"),
					  latitude_->value());
			settings.setValue(QStringLiteral("altitude"),
					  altitude_->value());
			settings.sync();
			if (!nickname_->text().trimmed().isEmpty())
				saveConfiguration(false);
		});
		auto scheduleAutoSave = [this] { autoSaveTimer_->start(); };
		connect(input_mode_,
			qOverload<int>(&QComboBox::currentIndexChanged), this,
			[scheduleAutoSave](int) { scheduleAutoSave(); });
		connect(
		    audio_device_,
		    qOverload<int>(&QComboBox::currentIndexChanged), this,
		    [this, scheduleAutoSave](int) {
			    scheduleAutoSave();
			    if (audio_device_->currentIndex() >= 0 &&
				input_mode_->currentData().toInt() != 2) {
				    notifyRunningDspAudioDevice(
					audio_device_->currentData().toInt());
			    }
		    });
		connect(nickname_, &QLineEdit::textChanged, this,
			[scheduleAutoSave](const QString &) {
				scheduleAutoSave();
			});
		connect(longitude_,
			qOverload<double>(&QDoubleSpinBox::valueChanged), this,
			[scheduleAutoSave](double) { scheduleAutoSave(); });
		connect(latitude_,
			qOverload<double>(&QDoubleSpinBox::valueChanged), this,
			[scheduleAutoSave](double) { scheduleAutoSave(); });
		connect(altitude_,
			qOverload<double>(&QDoubleSpinBox::valueChanged), this,
			[scheduleAutoSave](double) { scheduleAutoSave(); });
		connect(recording_enabled_, &QCheckBox::toggled, this,
			[scheduleAutoSave](bool) { scheduleAutoSave(); });
		content->adjustSize();
		// Size the launcher for the largest normal form state.  The
		// shared-IQ mode removes the sound-card row, but switching to
		// an audio mode must not suddenly require a vertical scrollbar.
		const int audioRowReserve =
		    audio_row_inserted_
			? 0
			: audio_device_->sizeHint().height() +
			      std::max(0, form_->verticalSpacing());
		const QSize desired =
		    content->sizeHint() + QSize(2, 2 + audioRowReserve);
		if (const QScreen *screen = QGuiApplication::primaryScreen()) {
			const QRect available = screen->availableGeometry();
			const int targetWidth = std::min(
			    desired.width(), int(available.width() * 0.92));
			const int targetHeight = std::min(
			    desired.height(), int(available.height() * 0.92));
			setMinimumSize(std::min(440, targetWidth),
				       targetHeight);
			resize(targetWidth, targetHeight);
		} else {
			setMinimumSize(std::min(440, desired.width()),
				       desired.height());
			resize(desired);
		}
	}

      private:
	void setStatusText(const QString &text)
	{
		status_text_ = text;
		refreshStatusElide();
	}

	void refreshStatusElide()
	{
		if (!status_label_)
			return;
		const int available = status_label_->width();
		if (available > 0) {
			const QString elided =
			    status_label_->fontMetrics().elidedText(
				status_text_, Qt::ElideMiddle, available);
			if (elided != status_label_->text())
				status_label_->setText(elided);
		} else {
			status_label_->setText(status_text_);
		}
	}

	bool eventFilter(QObject *watched, QEvent *event) override
	{
		if (watched == status_label_ && event->type() == QEvent::Resize)
			refreshStatusElide();
		return QWidget::eventFilter(watched, event);
	}

	void resizeEvent(QResizeEvent *event) override
	{
		QWidget::resizeEvent(event);
		refreshStatusElide();
	}

	bool inputModeIndexEnabled(int index) const
	{
		if (index < 0 || !input_mode_)
			return false;
		if (auto *model = qobject_cast<QStandardItemModel *>(
			input_mode_->model())) {
			if (auto *item = model->item(index))
				return item->isEnabled();
		}
		return true;
	}

	int firstEnabledInputModeIndex() const
	{
		if (!input_mode_)
			return 0;
		for (int index = 0; index < input_mode_->count(); ++index) {
			if (inputModeIndexEnabled(index))
				return index;
		}
		return 0;
	}

	void updateAudioDeviceVisibility()
	{
		const int mode = input_mode_->currentData().toInt();
		const bool visible = mode != 2;
		if (!form_ || !audio_device_label_)
			return;
		if (!visible && audio_row_inserted_) {
			const auto row = form_->takeRow(audio_device_);
			delete row.labelItem;
			delete row.fieldItem;
			audio_device_label_->hide();
			audio_device_->hide();
			audio_row_inserted_ = false;
		} else if (visible && !audio_row_inserted_) {
			audio_device_label_->show();
			audio_device_->show();
			form_->insertRow(1, audio_device_label_, audio_device_);
			audio_row_inserted_ = true;
		}
	}

	void refreshAudioDevices()
	{
		const QString previous =
		    audio_device_ ? audio_device_->currentText() : QString();
		const int previousId =
		    audio_device_ && audio_device_->currentIndex() >= 0
			? audio_device_->currentData().toInt()
			: -1;
		QList<AudioInputDevice> devices;
		int defaultChannels = 0;
#ifdef Q_OS_WIN
		WAVEINCAPSW defaultCaps{};
		if (waveInGetDevCapsW(WAVE_MAPPER, &defaultCaps,
				      sizeof(defaultCaps)) ==
		    MMSYSERR_NOERROR) {
			defaultChannels = int(defaultCaps.wChannels);
		}
#elif defined(Q_OS_MACOS)
		defaultChannels = asrtu::macDefaultInputChannels();
#endif
		devices.append(
		    {QCoreApplication::translate("ASRTU", "系统默认输入设备"),
		     -1, defaultChannels});
#ifdef Q_OS_WIN
		const UINT count = waveInGetNumDevs();
		for (UINT id = 0; id < count; ++id) {
			WAVEINCAPSW caps{};
			if (waveInGetDevCapsW(id, &caps, sizeof(caps)) ==
			    MMSYSERR_NOERROR) {
				devices.append(
				    {QString::fromWCharArray(caps.szPname),
				     int(id), int(caps.wChannels)});
			}
		}
#elif defined(Q_OS_MACOS)
		const auto macDevices = asrtu::macInputDevices();
		for (size_t id = 0; id < macDevices.size(); ++id)
			devices.append(
			    {QString::fromStdString(macDevices[id].name),
			     int(id), macDevices[id].channels});
#endif
		bool unchanged =
		    audio_device_ && audio_device_->count() == devices.size();
		if (unchanged) {
			for (int i = 0; i < devices.size(); ++i) {
				if (audio_device_->itemText(i) !=
					devices.at(i).name ||
				    audio_device_->itemData(i).toInt() !=
					devices.at(i).id ||
				    audio_device_
					    ->itemData(i,
						       kAudioChannelCountRole)
					    .toInt() !=
					devices.at(i).channels) {
					unchanged = false;
					break;
				}
			}
		}
		if (unchanged)
			return;

		QSignalBlocker blocker(audio_device_);
		audio_device_->clear();
		for (const auto &device : devices) {
			audio_device_->addItem(device.name, device.id);
			audio_device_->setItemData(audio_device_->count() - 1,
						   device.channels,
						   kAudioChannelCountRole);
		}
		QString wanted = previous;
		if (wanted.isEmpty()) {
			QSettings settings(
			    QStringLiteral("ASRTU"),
			    QStringLiteral("AstroSeriesLauncher"));
			wanted =
			    settings.value(QStringLiteral("audio_device_name"))
				.toString();
		}
		const int index =
		    audio_device_->findText(wanted, Qt::MatchFixedString);
		audio_device_->setCurrentIndex(index >= 0 ? index : 0);
		blocker.unblock();
		// Device enumeration is observational only. RDP logon/logoff
		// and USB hot-plug can reorder WinMM numeric IDs; treating that
		// reorder as a user selection used to stop and rebuild the GNU
		// Radio graph from the UI thread, which could freeze on a
		// disappearing endpoint. The currentIndexChanged handler still
		// applies an explicit user choice.
		(void)previousId;
	}

	QString savedSatellite() const
	{
		QSettings settings(QStringLiteral("ASRTU"),
				   QStringLiteral("AstroSeriesLauncher"));
		const QString saved = settings
					  .value(QStringLiteral("satellite"),
						 QStringLiteral("ASRTU-1"))
					  .toString();
		for (const auto &profile : satelliteProfiles()) {
			if (profile.name.compare(saved, Qt::CaseInsensitive) ==
			    0)
				return profile.name;
		}
		return satelliteProfiles().first().name;
	}

	QString chooseSatellite(bool *accepted, bool *satnogsEnabled)
	{
		QDialog dialog(this);
		dialog.setWindowTitle(
		    QCoreApplication::translate("ASRTU", "上传设置"));
		auto *layout = new QVBoxLayout(&dialog);
		auto *satellite = new QComboBox(&dialog);
		for (const auto &profile : satelliteProfiles())
			satellite->addItem(profile.name);
		satellite->setCurrentText(savedSatellite());
		auto *form = new QFormLayout;
		form->addRow(
		    QCoreApplication::translate("ASRTU", "本次遥测上传目标："),
		    satellite);
		layout->addLayout(form);
		auto *satnogs = new QCheckBox(
		    QCoreApplication::translate("ASRTU", "同时上传至 SatNOGS"),
		    &dialog);
		satnogs->setChecked(false);
		// Satellites without an MMT server only have the SatNOGS path.
		// Keep that path mandatory so accepting the dialog can never
		// leave the selected satellite without an upload destination.
		const auto refreshSatnogsDefault = [satnogs, satellite] {
			const QString name = satellite->currentText();
			const bool satnogsOnly =
			    webSocketForSatellite(name).isEmpty();
			satnogs->setChecked(satnogsOnly);
			satnogs->setEnabled(!satnogsOnly);
		};
		refreshSatnogsDefault();
		connect(
		    satellite, qOverload<int>(&QComboBox::currentIndexChanged),
		    this,
		    [refreshSatnogsDefault](int) { refreshSatnogsDefault(); });
		layout->addWidget(satnogs);
		auto *box = new QDialogButtonBox(
		    QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
		connect(box, &QDialogButtonBox::accepted, &dialog,
			&QDialog::accept);
		connect(box, &QDialogButtonBox::rejected, &dialog,
			&QDialog::reject);
		layout->addWidget(box);
		*accepted = dialog.exec() == QDialog::Accepted;
		*satnogsEnabled = *accepted && satnogs->isChecked();
		return satellite->currentText();
	}

	QDoubleSpinBox *makeCoordinate(double minimum, double maximum,
				       int decimals)
	{
		auto *value = new QDoubleSpinBox(this);
		value->setRange(minimum, maximum);
		value->setDecimals(decimals);
		value->setSingleStep(decimals > 2 ? 0.0001 : 1.0);
		value->setKeyboardTracking(false);
		return value;
	}

	void loadConfiguration()
	{
		QSettings settings(QStringLiteral("ASRTU"),
				   QStringLiteral("AstroSeriesLauncher"));
		const int savedInputMode =
		    normalizedInputMode(settings
					    .value(QStringLiteral("input_mode"),
						   input_mode_->itemData(0))
					    .toInt());
		int savedInputIndex = input_mode_->findData(savedInputMode);
		// A saved mode can become unavailable after an installation
		// change (for example the SDR bridge is no longer installed).
		// Never leave the disabled item current: startup would
		// otherwise run the decoder with an input that can never
		// produce samples.
		if (!inputModeIndexEnabled(savedInputIndex))
			savedInputIndex = firstEnabledInputModeIndex();
		input_mode_->setCurrentIndex(
		    savedInputIndex >= 0 ? savedInputIndex : 0);
		// Recording is deliberately opt-in for every launcher session
		// so an old preference cannot silently start filling the
		// records folder.
		recording_enabled_->setChecked(false);
		// Reset the optional choice for MMT-backed satellites. JAMX
		// enforces SatNOGS when the upload dialog is opened.
		settings.setValue(QStringLiteral("satnogs_enabled"), false);
		const QString savedAudio =
		    settings
			.value(QStringLiteral("audio_device_name"),
			       QCoreApplication::translate("ASRTU",
							   "系统默认输入设备"))
			.toString();
		const int audioIndex =
		    audio_device_->findText(savedAudio, Qt::MatchFixedString);
		audio_device_->setCurrentIndex(audioIndex >= 0 ? audioIndex
							       : 0);
		nickname_->setText(
		    settings.value(QStringLiteral("nickname")).toString());
		longitude_->setValue(
		    settings.value(QStringLiteral("longitude"), 0.0)
			.toDouble());
		latitude_->setValue(
		    settings.value(QStringLiteral("latitude"), 0.0).toDouble());
		altitude_->setValue(
		    settings.value(QStringLiteral("altitude"), 0.0).toDouble());
		updateAudioDeviceVisibility();
		QFile file(configPath());
		if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
			return;
		const QString text = QString::fromUtf8(file.readAll());
		auto textValue = [&text](const QString &key) {
			const QRegularExpression expression(
			    QStringLiteral(
				"(?:^|\\n)\\s*%1\\s*=\\s*\"([^\"]*)\"\\s*;")
				.arg(QRegularExpression::escape(key)));
			return expression.match(text).captured(1);
		};
		auto numberValue = [&text](const QString &key,
					   double fallback) {
			const QRegularExpression expression(
			    QStringLiteral(
				"(?:^|\\n)\\s*%1\\s*=\\s*([-+0-9.eE]+)\\s*;")
				.arg(QRegularExpression::escape(key)));
			const auto match = expression.match(text);
			bool ok = false;
			const double value = match.captured(1).toDouble(&ok);
			return ok ? value : fallback;
		};
		const QString configNickname =
		    textValue(QStringLiteral("proxy_nickname"));
		if (!configNickname.isEmpty())
			nickname_->setText(configNickname);
		const QString satellite = textValue(QStringLiteral("sat_name"));
		if (!satellite.isEmpty())
			settings.setValue(QStringLiteral("satellite"),
					  satellite);
		longitude_->setValue(numberValue(QStringLiteral("proxy_long"),
						 longitude_->value()));
		latitude_->setValue(numberValue(QStringLiteral("proxy_lat"),
						latitude_->value()));
		altitude_->setValue(numberValue(QStringLiteral("proxy_alt"),
						altitude_->value()));
	}

	bool saveConfiguration(bool notify, QString satellite = {})
	{
		const QString nickname = nickname_->text().trimmed();
		if (nickname.isEmpty()) {
			QMessageBox::warning(
			    this,
			    QCoreApplication::translate("ASRTU", "资料不完整"),
			    QCoreApplication::translate("ASRTU",
							"请填写呼号或昵称。"));
			nickname_->setFocus();
			return false;
		}
		QDir().mkpath(proxyDirectory());
		QSettings settings(QStringLiteral("ASRTU"),
				   QStringLiteral("AstroSeriesLauncher"));
		if (satellite.isEmpty())
			satellite = savedSatellite();
		settings.setValue(QStringLiteral("satellite"), satellite);
		settings.setValue(QStringLiteral("nickname"), nickname);
		settings.setValue(QStringLiteral("longitude"),
				  longitude_->value());
		settings.setValue(QStringLiteral("latitude"),
				  latitude_->value());
		settings.setValue(QStringLiteral("altitude"),
				  altitude_->value());
		settings.sync();
		QSaveFile file(configPath());
		if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
			QMessageBox::critical(
			    this,
			    QCoreApplication::translate("ASRTU", "保存失败"),
			    QCoreApplication::translate("ASRTU",
							"无法写入：\n%1")
				.arg(configPath()));
			return false;
		}
		QTextStream stream(&file);
		stream.setCodec("UTF-8");
		const QString webSocket = webSocketForSatellite(satellite);
		stream << "# Server Config\n"
		       << "zmq_address = \"tcp://127.0.0.1:5555\";\n"
		       << "ws_address = \"" << webSocket << "\";\n"
		       << "ws_port = 9000;\n\n"
		       << "# Satellite Config\n"
		       << "sat_name = \"" << satellite << "\";\n\n"
		       << "# Proxy Config\n"
		       << "physical_channel = 0;\n"
		       << "proxy_nickname = \"" << escapedConfigString(nickname)
		       << "\";\n"
		       << "proxy_long = "
		       << QString::number(longitude_->value(), 'f', 6) << ";\n"
		       << "proxy_lat = "
		       << QString::number(latitude_->value(), 'f', 6) << ";\n"
		       << "proxy_alt = "
		       << QString::number(altitude_->value(), 'f', 2) << ";\n";
		stream.flush();
		if (!file.commit()) {
			QMessageBox::critical(
			    this,
			    QCoreApplication::translate("ASRTU", "保存失败"),
			    QCoreApplication::translate("ASRTU",
							"配置文件提交失败。"));
			return false;
		}
		if (notify && status_label_)
			setStatusText(
			    QCoreApplication::translate("ASRTU", "设置已保存"));
		return true;
	}

	QLineEdit *nickname_ = nullptr;
	QDoubleSpinBox *longitude_ = nullptr;
	QDoubleSpinBox *latitude_ = nullptr;
	QDoubleSpinBox *altitude_ = nullptr;
	QComboBox *input_mode_ = nullptr;
	QComboBox *audio_device_ = nullptr;
	QWidget *audio_device_label_ = nullptr;
	QFormLayout *form_ = nullptr;
	bool audio_row_inserted_ = true;
	QCheckBox *recording_enabled_ = nullptr;
	QTimer *autoSaveTimer_ = nullptr;
	QLabel *status_label_ = nullptr;
	QString status_text_;
};
} // namespace

int main(int argc, char *argv[])
{
	QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
	QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
	QCoreApplication::setAttribute(Qt::AA_DisableWindowContextHelpButton);
#endif
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
	QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
	    Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif
	QApplication application(argc, argv);
	QTranslator translator;
	installSystemTranslation(application, translator);
	QFont uiFont(QStringLiteral("Microsoft YaHei UI"), 9);
	uiFont.setStyleStrategy(static_cast<QFont::StyleStrategy>(
	    QFont::PreferAntialias | QFont::PreferQuality));
	uiFont.setHintingPreference(QFont::PreferFullHinting);
	application.setFont(uiFont);
	QCoreApplication::setOrganizationName(QStringLiteral("ASRTU"));
	QCoreApplication::setApplicationName(QStringLiteral("ASRTU1_Launcher"));

	if (application.arguments().contains(
		QStringLiteral("--check-session-directory"))) {
		QString error;
		const QString directory = createSessionDirectory(&error);
		if (directory.isEmpty()) {
			std::cerr << error.toUtf8().constData() << '\n';
			return 1;
		}
		std::cout << directory.toUtf8().constData() << '\n';
		return 0;
	}

	if (application.arguments().contains(QStringLiteral("--start"))) {
		QString error;
		QString sessionDirectory;
		qint64 proxyProcessId = 0;
		if (!startSuite(true, 0, {}, false, -1, true, &sessionDirectory,
				&proxyProcessId, &error)) {
			QMessageBox::critical(
			    nullptr,
			    QCoreApplication::translate(
				"ASRTU", "阿斯图系列卫星启动失败"),
			    error);
			return 1;
		}
		return 0;
	}

	if (application.arguments().contains(QStringLiteral("--sdrsharp"))) {
		QString error;
		const QList<SdrProgram> programs = availableSdrPrograms();
		const auto program = std::find_if(
		    programs.cbegin(), programs.cend(),
		    [](const SdrProgram &candidate) {
			    return candidate.overrideKey ==
				       QStringLiteral("sdrsharp") &&
				   sdrExecutableAvailable(candidate.executable);
		    });
		qint64 processId = 0;
		if (program == programs.cend() ||
		    !startSdrProgram(*program, &processId, &error)) {
			QMessageBox::critical(
			    nullptr,
			    QCoreApplication::translate("ASRTU",
							"SDR# 启动失败"),
			    error.isEmpty()
				? QCoreApplication::translate(
				      "ASRTU", "未找到完整的 SDR# 安装。")
				: error);
			return 1;
		}
		return 0;
	}

	if (application.arguments().contains(QStringLiteral("--sdrpp"))) {
		QString error;
		const QList<SdrProgram> programs = availableSdrPrograms();
		const auto program = std::find_if(
		    programs.cbegin(), programs.cend(),
		    [](const SdrProgram &candidate) {
			    return candidate.overrideKey ==
				       QStringLiteral("sdrpp") &&
				   sdrExecutableAvailable(candidate.executable);
		    });
		qint64 processId = 0;
		if (program == programs.cend() ||
		    !startSdrProgram(*program, &processId, &error)) {
			QMessageBox::critical(
			    nullptr,
			    QCoreApplication::translate("ASRTU",
							"SDR++ 启动失败"),
			    error.isEmpty()
				? QCoreApplication::translate(
				      "ASRTU", "未找到完整的随包 SDR++ 安装。")
				: error);
			return 1;
		}
		return 0;
	}

	LauncherWindow window;
	window.show();
	const int screenshotOption =
	    application.arguments().indexOf(QStringLiteral("--screenshot"));
	if (screenshotOption >= 0 &&
	    screenshotOption + 1 < application.arguments().size()) {
		const QString path =
		    application.arguments().at(screenshotOption + 1);
		QTimer::singleShot(300, &window, [&application, &window, path] {
			window.grab().save(path);
			application.quit();
		});
	}
	return application.exec();
}
