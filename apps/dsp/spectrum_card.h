#pragma once

#include "ini_settings.h"

#include <QFrame>
#include <QResizeEvent>
#include <QSplitter>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

// Overlay the control on the plot; only a collapsed spectrum needs a header.
class SpectrumCard final : public QFrame
{
      public:
	SpectrumCard(QWidget *plot, const QString &title, const QString &key,
		     QSplitter *splitter)
	    : QFrame(splitter), plot_(plot), button_(new QToolButton(this)),
	      title_(title), key_(key), splitter_(splitter)
	{
		setObjectName(QStringLiteral("spectrumCard"));
		auto *layout = new QVBoxLayout(this);
		layout->setContentsMargins(0, 0, 0, 0);
		layout->addWidget(plot_);
		button_->setObjectName(QStringLiteral("spectrumToggle"));
		button_->setCheckable(true);
		button_->setAutoRaise(true);
		button_->setText(title_);
		button_->setAccessibleName(title_);
		asrtu::IniSettings settings(QStringLiteral("decoder"));
		button_->setChecked(settings.value(key_, true).toBool());
		applyExpanded(button_->isChecked());
		connect(button_, &QToolButton::toggled, this,
			[this](bool expanded) {
				if (!expanded)
					expanded_height_ = height();
				applyExpanded(expanded);
				auto sizes = splitter_->sizes();
				const int index = splitter_->indexOf(this);
				if (index >= 0 && sizes.size() == 2) {
					const int total = sizes[0] + sizes[1];
					sizes[index] =
					    expanded ? expanded_height_ : 24;
					sizes[1 - index] =
					    std::max(1, total - sizes[index]);
					splitter_->setSizes(sizes);
				}
				asrtu::IniSettings settings(
				    QStringLiteral("decoder"));
				settings.setValue(key_, expanded);
			});
	}

      protected:
	void resizeEvent(QResizeEvent *event) override
	{
		QFrame::resizeEvent(event);
		positionButton();
	}

      private:
	void applyExpanded(bool expanded)
	{
		plot_->setVisible(expanded);
		setMinimumHeight(expanded ? plot_->minimumHeight() : 24);
		setMaximumHeight(expanded ? QWIDGETSIZE_MAX : 24);
		button_->setArrowType(expanded ? Qt::DownArrow
					       : Qt::RightArrow);
		button_->setToolButtonStyle(expanded
						? Qt::ToolButtonIconOnly
						: Qt::ToolButtonTextBesideIcon);
		button_->setToolTip((expanded ? QStringLiteral("Collapse ")
					      : QStringLiteral("Expand ")) +
				    title_);
		positionButton();
		updateGeometry();
	}
	void positionButton()
	{
		const bool expanded = button_->isChecked();
		button_->setFixedSize(
		    expanded ? 24 : std::max(24, width() - 12), 22);
		button_->move(expanded ? std::max(0, width() - 28) : 6, 1);
		button_->raise();
	}
	QWidget *plot_;
	QToolButton *button_;
	QString title_;
	QString key_;
	QSplitter *splitter_;
	int expanded_height_ = 300;
};
