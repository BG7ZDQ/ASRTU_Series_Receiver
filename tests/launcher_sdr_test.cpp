// Exercise the real chooser with the real parent stylesheet; a standalone
// dialog misses the inherited button minimum-height regression.
#define main asrtuLauncherMain
#include "../apps/launcher/suite_launcher.cpp"
#undef main
#include <QTemporaryDir>
#include <QFontDatabase>

bool testSdrppPresetMigration()
{
	QTemporaryDir temp;
	if (!temp.isValid())
		return false;
	const QDir root(temp.path());
	if (!QDir().mkpath(root.filePath(QStringLiteral("modules"))))
		return false;
	QFile plugin(root.filePath(QStringLiteral("modules/asrtu_sink.dll")));
	if (!plugin.open(QIODevice::WriteOnly))
		return false;
	plugin.close();

	QJsonObject config{
	    {QStringLiteral("moduleInstances"),
	     QJsonObject{
		 {QStringLiteral("ASRTU Bridge"),
		  QJsonObject{{QStringLiteral("enabled"), false},
			      {QStringLiteral("module"),
			       QStringLiteral("asrtu_sink")}}},
		 {QStringLiteral("Rigctl Server"),
		  QJsonObject{{QStringLiteral("enabled"), true},
			      {QStringLiteral("module"),
			       QStringLiteral("rigctl_server")}}},
	     }},
	    {QStringLiteral("streams"),
	     QJsonObject{
		 {QStringLiteral("Radio"),
		  QJsonObject{
		      {QStringLiteral("sink"), QStringLiteral("Network")}}},
	     }},
	};
	QJsonObject radio{
	    {QStringLiteral("Radio"),
	     QJsonObject{
		 {QStringLiteral("selectedDemodId"), 0},
		 {QStringLiteral("RAW"),
		  QJsonObject{{QStringLiteral("bandwidth"), 200000.0}}}}},
	};
	QJsonObject bridge{
	    {QStringLiteral("Radio"),
	     QJsonObject{{QStringLiteral("autoDoppler"), false}}},
	};
	QString error;
	if (!writeJsonObject(root.filePath(QStringLiteral("config.json")),
			     config, &error) ||
	    !writeJsonObject(root.filePath(QStringLiteral("radio_config.json")),
			     radio, &error) ||
	    !writeJsonObject(
		root.filePath(QStringLiteral("asrtu_sink_config.json")), bridge,
		&error) ||
	    !configureSdrppPreset(root.absolutePath(), &error))
		return false;

	if (!readJsonObject(root.filePath(QStringLiteral("config.json")),
			    &config, &error) ||
	    !readJsonObject(root.filePath(QStringLiteral("radio_config.json")),
			    &radio, &error) ||
	    !readJsonObject(
		root.filePath(QStringLiteral("asrtu_sink_config.json")),
		&bridge, &error))
		return false;
	const QJsonObject instances =
	    config.value(QStringLiteral("moduleInstances")).toObject();
	const QJsonObject streams =
	    config.value(QStringLiteral("streams")).toObject();
	const QJsonObject radioSettings =
	    radio.value(QStringLiteral("Radio")).toObject();
	const QJsonObject bridgeSettings =
	    bridge.value(QStringLiteral("Radio")).toObject();
	if (config.value(QStringLiteral("centerTuning")).toBool(true) ||
	    !instances.value(QStringLiteral("ASRTU Bridge"))
		 .toObject()
		 .value(QStringLiteral("enabled"))
		 .toBool() ||
	    !instances.value(QStringLiteral("Radio")).isObject() ||
	    !streams.value(QStringLiteral("Radio"))
		 .toObject()
		 .contains(QStringLiteral("volume")) ||
	    !streams.value(QStringLiteral("Radio"))
		 .toObject()
		 .contains(QStringLiteral("muted")) ||
	    streams.value(QStringLiteral("Radio"))
		    .toObject()
		    .value(QStringLiteral("sink"))
		    .toString() != QStringLiteral("ASRTU Receiver") ||
	    radioSettings.value(QStringLiteral("selectedDemodId")).toInt() !=
		7 ||
	    radioSettings.value(QStringLiteral("RAW"))
		    .toObject()
		    .value(QStringLiteral("bandwidth"))
		    .toDouble() != 48000.0 ||
	    !bridgeSettings.value(QStringLiteral("autoDoppler")).toBool())
		return false;
	QFile marker(root.filePath(QStringLiteral("asrtu-preset.version")));
	if (!marker.open(QIODevice::ReadOnly) ||
	    marker.readAll().trimmed().toInt() != 3)
		return false;

	// A later operator choice must survive subsequent launcher starts.
	QJsonObject stream = streams.value(QStringLiteral("Radio")).toObject();
	stream[QStringLiteral("sink")] = QStringLiteral("Network");
	QJsonObject changedStreams = streams;
	changedStreams[QStringLiteral("Radio")] = stream;
	config[QStringLiteral("streams")] = changedStreams;
	if (!writeJsonObject(root.filePath(QStringLiteral("config.json")),
			     config, &error) ||
	    !configureSdrppPreset(root.absolutePath(), &error) ||
	    !readJsonObject(root.filePath(QStringLiteral("config.json")),
			    &config, &error))
		return false;
	return config.value(QStringLiteral("streams"))
		   .toObject()
		   .value(QStringLiteral("Radio"))
		   .toObject()
		   .value(QStringLiteral("sink"))
		   .toString() == QStringLiteral("Network");
}

int main(int argc, char **argv)
{
	QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
	QApplication app(argc, argv);
#ifdef Q_OS_WIN
	// The offscreen Qt backend does not enumerate Windows system fonts.
	QFontDatabase::addApplicationFont(
	    QStringLiteral("C:/Windows/Fonts/msyh.ttc"));
#endif
	QStandardPaths::setTestModeEnabled(true);
	QTemporaryDir settings;
	QSettings::setDefaultFormat(QSettings::IniFormat);
	QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
			   settings.path());
	QCoreApplication::setOrganizationName(QStringLiteral("ASRTU_Test"));
	QCoreApplication::setApplicationName(QStringLiteral("Chooser"));
	app.setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 9));
	bool ok = testSdrppPresetMigration();
	const QIcon sdrppIcon = sdrProgramIcon(
	    {QStringLiteral("SDR++"), QString(), {}, {}, false, {}, -1});
	if (sdrppIcon.pixmap(32, 32).isNull()) {
		std::cerr << "SDR++ icon resource is missing\n";
		ok = false;
	}
	const QIcon sdrsharpIcon = sdrProgramIcon(
	    {QStringLiteral("SDR#"), QString(), {}, {}, true, {}, -1});
	if (sdrsharpIcon.pixmap(32, 32).isNull()) {
		std::cerr << "SDR# icon resource is missing\n";
		ok = false;
	}
#ifdef Q_OS_WIN
	if (normalizedInputMode(3) != 2) {
		std::cerr << "Legacy SDR++ input mode was not normalized\n";
		ok = false;
	}
#endif
	LauncherWindow parent;
	parent.show();
#ifdef Q_OS_WIN
	auto *inputMode =
	    parent.findChild<QComboBox *>(QStringLiteral("inputMode"));
	if (!inputMode || inputMode->findData(2) < 0 ||
	    inputMode->findData(3) >= 0 ||
	    inputMode->itemText(inputMode->findData(2)).isEmpty() ||
	    inputMode->itemText(inputMode->findData(2))
		.contains(QStringLiteral("SDR#")) ||
	    inputMode->itemText(inputMode->findData(2))
		.contains(QStringLiteral("SDR++"))) {
		std::cerr << "Unified shared-memory input label is invalid\n";
		ok = false;
	}
#endif
	QTimer::singleShot(200, &app, [&] {
		auto *dialog =
		    qobject_cast<QDialog *>(QApplication::activeModalWidget());
		if (!dialog) {
			ok = false;
			app.quit();
			return;
		}
		if (argc > 1)
			dialog->grab().save(QString::fromLocal8Bit(argv[1]));
		for (auto *label : dialog->findChildren<QLabel *>()) {
			if (label->objectName() != QStringLiteral("sdrName") &&
			    label->objectName() !=
				QStringLiteral("sdrDescription") &&
			    label->objectName() != QStringLiteral("sdrStatus"))
				continue;
			std::cout << label->objectName().toStdString() << ": "
				  << label->width() << " x " << label->height()
				  << " (font " << label->fontMetrics().height()
				  << ")\n";
			if (label->height() < label->fontMetrics().height())
				ok = false;
			if (!label->parentWidget()->rect().contains(
				label->geometry()))
				ok = false;
		}
		dialog->reject();
	});
	showSdrChooser(&parent);
	return ok ? 0 : 1;
}
