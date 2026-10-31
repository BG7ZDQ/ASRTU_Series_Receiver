// Exercises the real launcher window so the locator row keeps pointing at the
// same decimal station fields the rest of the suite consumes.
#define main asrtuLauncherMain
#include "../apps/launcher/suite_launcher.cpp"
#undef main

#include <QApplication>
#include <QDoubleSpinBox>
#include <QFontDatabase>
#include <QLineEdit>
#include <QTemporaryDir>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <iostream>

namespace
{

bool almostEqual(double actual, double expected)
{
	return std::fabs(actual - expected) <= 1e-5;
}

void emitEditingFinished(QLineEdit *field)
{
	assert(QMetaObject::invokeMethod(field, "editingFinished"));
	QCoreApplication::processEvents();
}

} // namespace

int main(int argc, char **argv)
{
	QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
	QApplication application(argc, argv);
#ifdef Q_OS_WIN
	// The offscreen Qt backend does not enumerate Windows system fonts.
	QFontDatabase::addApplicationFont(
	    QStringLiteral("C:/Windows/Fonts/msyh.ttc"));
#endif
	QStandardPaths::setTestModeEnabled(true);
	QTemporaryDir settings;
	assert(settings.isValid());
	qputenv("ASRTU_CONFIG_DIR", settings.path().toUtf8());
	QSettings::setDefaultFormat(QSettings::NativeFormat);
	QCoreApplication::setOrganizationName(QStringLiteral("ASRTU_Test"));
	QCoreApplication::setApplicationName(QStringLiteral("GridLocator"));

	LauncherWindow window;
	window.show();
	auto *locator =
	    window.findChild<QLineEdit *>(QStringLiteral("gridLocator"));
	auto *longitude = window.findChild<QDoubleSpinBox *>(
	    QStringLiteral("stationLongitude"));
	auto *latitude = window.findChild<QDoubleSpinBox *>(
	    QStringLiteral("stationLatitude"));
	assert(locator);
	assert(longitude);
	assert(latitude);

	// A station that was never configured has no locator to show.
	assert(locator->text().isEmpty());
	assert(locator->property("gridInvalid").toBool() == false);

	// A locator typed by the operator updates the decimal fields.
	locator->setFocus();
	locator->setText(QStringLiteral("ol 63-pd"));
	emitEditingFinished(locator);
	assert(locator->text() == QStringLiteral("OL63PD"));
	assert(almostEqual(latitude->value(), 23.145833));
	assert(almostEqual(longitude->value(), 113.291667));
	assert(locator->property("gridInvalid").toBool() == false);

	// Editing the decimal fields rewrites the locator without touching
	// them.
	longitude->setValue(113.2644);
	latitude->setValue(23.1291);
	assert(locator->text() == QStringLiteral("OL63PD"));
	assert(almostEqual(longitude->value(), 113.2644));
	assert(almostEqual(latitude->value(), 23.1291));

	// Invalid input must flag the field and leave the station untouched.
	locator->setText(QStringLiteral("ZZ99"));
	emitEditingFinished(locator);
	assert(locator->property("gridInvalid").toBool() == true);
	assert(almostEqual(longitude->value(), 113.2644));
	assert(almostEqual(latitude->value(), 23.1291));

	// Editing the decimals regenerates a valid locator, so the rejected
	// entry must stop being styled as an error.
	longitude->setValue(113.3);
	assert(locator->property("gridInvalid").toBool() == false);
	assert(locator->text() == QStringLiteral("OL63PD"));
	assert(almostEqual(longitude->value(), 113.3));
	longitude->setValue(113.2644);

	// Extended (8 character) locators are accepted and preserved.
	locator->setText(QStringLiteral("om89ev12"));
	emitEditingFinished(locator);
	assert(locator->property("gridInvalid").toBool() == false);
	assert(locator->text() == QStringLiteral("OM89EV12"));
	assert(asrtu::grid::encode(latitude->value(), longitude->value(), 8) ==
	       QStringLiteral("OM89EV12"));

	// Six character entry drops back to the operator's chosen precision.
	locator->setText(QStringLiteral("OM89EV"));
	emitEditingFinished(locator);
	assert(locator->text() == QStringLiteral("OM89EV"));
	assert(almostEqual(latitude->value(), 39.895833));
	assert(almostEqual(longitude->value(), 116.375));

	if (argc > 1 && !window.grab().save(QString::fromLocal8Bit(argv[1]))) {
		std::cerr << "Unable to save the launcher screenshot\n";
		return 1;
	}
	return 0;
}