#pragma once

#include <QSplitter>
#include <QResizeEvent>
#include <algorithm>

class ResponsivePlotSplitter final : public QSplitter
{
      public:
	explicit ResponsivePlotSplitter(QWidget *parent = nullptr)
	    : QSplitter(Qt::Vertical, parent)
	{
		connect(this, &QSplitter::splitterMoved, this,
			[this] { manually_adjusted_ = true; });
	}

      protected:
	void resizeEvent(QResizeEvent *event) override
	{
		QSplitter::resizeEvent(event);
		const int available = height() - handleWidth();
		if (manually_adjusted_ || available <= 0 || count() < 2 ||
		    widget(1)->maximumHeight() <= 24)
			return;
		// The Qwt constellation widget needs roughly the right-column
		// width as its outer height for the inner -2..2 canvas to
		// appear square.
		const int maxConstellationHeight =
		    std::max(220, std::min(430, available - 150));
		const int constellationHeight =
		    std::clamp(width() - 20, 220, maxConstellationHeight);
		setSizes(
		    {constellationHeight, available - constellationHeight});
	}

      private:
	bool manually_adjusted_ = false;
};

class ResponsiveMainSplitter final : public QSplitter
{
      public:
	explicit ResponsiveMainSplitter(QWidget *parent = nullptr)
	    : QSplitter(Qt::Horizontal, parent)
	{
		connect(this, &QSplitter::splitterMoved, this,
			[this] { manually_adjusted_ = true; });
	}

      protected:
	void resizeEvent(QResizeEvent *event) override
	{
		QSplitter::resizeEvent(event);
		const int available = width() - handleWidth();
		if (manually_adjusted_ || available <= 0 || count() < 2)
			return;

		// Share narrow windows evenly; leave extra width to the
		// waterfall on large screens until the user chooses their own
		// split.
		const int rightWidth = std::clamp(available / 2, 340, 520);
		setSizes({available - rightWidth, rightWidth});
	}

      private:
	bool manually_adjusted_ = false;
};
