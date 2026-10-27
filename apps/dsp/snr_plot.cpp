#include "snr_plot.h"

#include <QPainter>
#include <algorithm>
#include <cmath>
#include <limits>

SnrPlot::SnrPlot(QWidget* parent) : QWidget(parent)
{
    setMinimumHeight(112);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

bool SnrPlot::addValue(double value, double audioSeconds, bool synchronized)
{
	if (!history_.add(value, audioSeconds, synchronized))
		return false;
	update();
	return true;
}

void SnrPlot::clear()
{
	history_.clear();
	scale_ = {};
	update();
}

void SnrPlot::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor(250, 252, 255));

    constexpr int left = 45, right = 12, top = 12, bottom = 25;
    const int w = std::max(1, width() - left - right);
    const int h = std::max(1, height() - top - bottom);

    double ymin = -5.0, ymax = 20.0;
    QVector<double> finiteValues;
    finiteValues.reserve(int(history_.points().size()));
    for (const auto &point : history_.points())
	    finiteValues.append(point.value);
    if (!finiteValues.isEmpty()) {
        const auto mm = std::minmax_element(finiteValues.cbegin(), finiteValues.cend());
        const double vmin = *mm.first, vmax = *mm.second;
	const auto candidate = snrScale(vmin, vmax);
	// Expand immediately; shrink only by at least half to avoid jitter.
	if (candidate.minimum < scale_.minimum ||
	    candidate.maximum > scale_.maximum ||
	    candidate.maximum - candidate.minimum <=
		(scale_.maximum - scale_.minimum) * 0.5)
		scale_ = candidate;
    } else {
	    scale_ = {};
    }
    ymin = scale_.minimum;
    ymax = scale_.maximum;

    QFont plotFont(QStringLiteral("Microsoft YaHei UI"), 8);
    plotFont.setStyleStrategy(static_cast<QFont::StyleStrategy>(
        QFont::PreferAntialias | QFont::PreferQuality));
    p.setFont(plotFont);
    const int intervals = int(std::lround((ymax - ymin) / scale_.step));
    for (int i = 0; i <= intervals; ++i) {
	    const double y = top + i * h / double(intervals);
	    const double value = ymax - i * scale_.step;
	    p.setPen(QPen(value == 0.0 ? QColor(120, 130, 145)
				       : QColor(205, 210, 218),
			  value == 0.0 ? 1.5 : 1.0));
	    p.drawLine(QPointF(left, y), QPointF(left + w, y));
	    p.setPen(QPen(QColor(60, 65, 70), 1));
	    p.drawText(QRectF(0, y - 8, left - 5, 16),
		       Qt::AlignRight | Qt::AlignVCenter,
		       QString::number(value, 'f', 0));
    }
    p.setPen(QPen(QColor(215, 220, 228), 1));
    for (int i = 0; i < 7; ++i) {
        const double x = left + i * w / 6.0;
        p.drawLine(QPointF(x, top), QPointF(x, top + h));
    }
    p.setPen(QPen(QColor(80, 85, 90), 1));
    p.drawRect(left, top, w, h);
    p.drawText(4, 10, QStringLiteral("dB"));
    p.drawText(QRectF(left, top + h + 3, w, 18), Qt::AlignCenter,
	       QStringLiteral("60 s"));

    if (finiteValues.size() < 2)
	    return;
    // Es/N0 is green, SVR is blue. Draw one segment per pair so the color
    // changes exactly at the algorithm transition without breaking the line.
    QPointF previous;
    bool hasPrevious = false;
    for (const auto &point : history_.points()) {
	    const QPointF current(left + history_.position(point) * w,
				  top + (ymax - point.value) * h /
					    std::max(0.001, ymax - ymin));
	    if (hasPrevious) {
		    QPen pen(point.synchronized ? QColor(0, 150, 70)
						: QColor(0, 110, 220),
			     2);
		    pen.setCapStyle(Qt::RoundCap);
		    pen.setJoinStyle(Qt::RoundJoin);
		    p.setPen(pen);
		    p.drawLine(previous, current);
	    }
	    previous = current;
	    hasPrevious = true;
    }
}
