#include "spectrum_card.h"
#include "responsive_splitter.h"
#include <QApplication>
#include <QLabel>
#include "status_layout.h"
#include <QTemporaryDir>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

int main(int argc, char **argv)
{
	QApplication app(argc, argv);
	// Small windows must share width; a user-selected split survives
	// resizing.
	ResponsiveMainSplitter mainSplit;
	mainSplit.setChildrenCollapsible(false);
	auto *left = new QWidget(&mainSplit);
	auto *right = new QWidget(&mainSplit);
	left->setMinimumWidth(340);
	right->setMinimumWidth(340);
	mainSplit.resize(820, 600);
	mainSplit.show();
	app.processEvents();
	assert(std::abs(left->width() - right->width()) <= 2);
	mainSplit.resize(1400, 600);
	app.processEvents();
	assert(right->width() <= 520);
	mainSplit.setSizes({700, 696});
	mainSplit.splitterMoved(700, 1);
	mainSplit.resize(1600, 600);
	app.processEvents();
	assert(right->width() >
	       650); // Must not reset to the automatic 520 px cap.

	ResponsivePlotSplitter plots;
	auto *constellation = new QWidget(&plots);
	auto *spectrum = new QWidget(&plots);
	constellation->setMinimumHeight(220);
	spectrum->setMinimumHeight(150);
	plots.resize(400, 800);
	plots.show();
	app.processEvents();
	plots.setSizes({250, 546});
	plots.splitterMoved(250, 1);
	plots.resize(500, 800);
	app.processEvents();
	assert(constellation->height() < 300);

	QWidget statusCard;
	auto *status = new StatusLayout(&statusCard);
	auto *snr = new QLabel("SNR (SVR): 23.23 dB");
	auto *frequency = new QLabel("Loop df: -2880.6 Hz");
	auto *lock = new QLabel("UNLOCKED");
	lock->setMinimumWidth(115);
	for (auto *label : {snr, frequency, lock})
		status->addWidget(label);
	statusCard.show();
	const int fullWidth = status->sizeHint().width();
	statusCard.resize(fullWidth + 20,
			  status->heightForWidth(fullWidth + 20));
	app.processEvents();
	assert(snr->y() == frequency->y());
	const int narrowWidth = status->minimumSize().width();
	statusCard.resize(narrowWidth, status->heightForWidth(narrowWidth));
	app.processEvents();
	assert(frequency->y() > snr->y());
	assert(!frequency->geometry().intersects(lock->geometry()));
	statusCard.resize(fullWidth + 20,
			  status->heightForWidth(fullWidth + 20));
	app.processEvents();
	assert(snr->y() == frequency->y());
	frequency->setText("Loop df: -1234567890.123456789 Hz");
	app.processEvents();
	status->setGeometry(statusCard.rect());
	assert(frequency->y() > snr->y());

	// In the single-row layout both value columns keep their width, so a
	// new value cannot shift the neighbouring field sideways.
	const int wideWidth = status->sizeHint().width() + 20;
	statusCard.resize(wideWidth, status->heightForWidth(wideWidth));
	app.processEvents();
	assert(snr->y() == frequency->y());
	const int snrWidth = snr->width();
	const int loopWidth = frequency->width();
	frequency->setText("Loop df: -1 Hz");
	app.processEvents();
	status->setGeometry(statusCard.rect());
	assert(snr->width() == snrWidth);
	assert(frequency->width() == loopWidth);

	// Between the short and the long single-row widths the row mode must
	// stay put while the value changes back and forth. Switching on the
	// live text width used to make the card oscillate between one and two
	// rows.
	statusCard.resize(fullWidth + 20,
			  status->heightForWidth(fullWidth + 20));
	app.processEvents();
	assert(frequency->y() > snr->y());
	frequency->setText("Loop df: -2880.6 Hz");
	app.processEvents();
	status->setGeometry(statusCard.rect());
	assert(frequency->y() > snr->y());
	frequency->setText("Loop df: -1234567890.123456789 Hz");
	app.processEvents();
	status->setGeometry(statusCard.rect());
	assert(frequency->y() > snr->y());

	QTemporaryDir temporary;
	assert(temporary.isValid());
	qputenv("ASRTU_CONFIG_DIR", temporary.path().toUtf8());
	QSettings::setDefaultFormat(QSettings::NativeFormat);
	for (const auto &key : {QStringLiteral("plots/agcSpectrumVisible"),
				QStringLiteral("plots/loopSpectrumVisible")}) {
		QSplitter splitter(Qt::Vertical);
		splitter.setChildrenCollapsible(false);
		auto *sibling = new QWidget(&splitter);
		sibling->setMinimumHeight(150);
		auto *plot = new QWidget;
		plot->setMinimumHeight(150);
		auto *card = new SpectrumCard(plot, QStringLiteral("Spectrum"),
					      key, &splitter);
		splitter.resize(600, 800);
		splitter.show();
		app.processEvents();
		auto *button = card->findChild<QToolButton *>();
		assert(button && button->isChecked() && plot->isVisible());
		assert(card->rect().contains(button->geometry()));
		const int siblingHeight = sibling->height();
		button->click();
		app.processEvents();
		assert(!plot->isVisible() && button->isVisible());
		assert(card->height() == 24 &&
		       sibling->height() > siblingHeight);
		splitter.resize(800, 1000);
		app.processEvents();
		assert(card->height() == 24);
		// A newly created module restores the persisted collapsed
		// choice.
		QSplitter restored(Qt::Vertical);
		auto *restoredPlot = new QWidget;
		auto *restoredCard = new SpectrumCard(
		    restoredPlot, QStringLiteral("Spectrum"), key, &restored);
		assert(!restoredCard->findChild<QToolButton *>()->isChecked());
		assert(restoredPlot->isHidden());
		button->click();
		app.processEvents();
		assert(plot->isVisible() && card->height() >= 150);
		assert(card->rect().contains(button->geometry()));
		assert(button->toolButtonStyle() == Qt::ToolButtonIconOnly);
	}
}
