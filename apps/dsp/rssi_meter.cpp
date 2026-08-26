#include "rssi_meter.h"

#include <QPainter>
#include <QPaintEvent>
#include <QLinearGradient>
#include <QSizePolicy>

#include <algorithm>
#include <cmath>

RssiMeter::RssiMeter(QWidget* parent) : QWidget(parent)
{
    setMinimumHeight(54);
    setMaximumHeight(58);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void RssiMeter::setReading(double value, double minimum, double maximum)
{
    value_ = value;
    minimum_ = minimum;
    maximum_ = std::max(maximum, minimum + 1.0);
    update();
}

void RssiMeter::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.fillRect(rect(), QColor(255, 255, 255));
    painter.setPen(QColor(36, 52, 71));

    const QRect area = rect().adjusted(5, 1, -5, -2);
    painter.drawText(area.left(), area.top(), area.width(), 15,
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("RSSI: %1 dB").arg(value_, 0, 'f', 2));

    const int scaleY = area.top() + 15;
    const QString low = QString::number(minimum_, 'f', 1);
    const QString mid = QString::number((minimum_ + maximum_) * 0.5, 'f', 1);
    const QString high = QString::number(maximum_, 'f', 1);
    painter.drawText(area.left(), scaleY, area.width(), 14,
                     Qt::AlignLeft | Qt::AlignVCenter, low);
    painter.drawText(area.left(), scaleY, area.width(), 14,
                     Qt::AlignHCenter | Qt::AlignVCenter, mid);
    painter.drawText(area.left(), scaleY, area.width(), 14,
                     Qt::AlignRight | Qt::AlignVCenter, high);

    const QRect bar(area.left(), area.top() + 34, area.width(), 13);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(226, 232, 240));
    painter.drawRoundedRect(bar, 5, 5);

    const double ratio = std::clamp((value_ - minimum_) / (maximum_ - minimum_),
                                    0.0, 1.0);
    const int innerWidth = std::max(0, bar.width() - 3);
    const int fillWidth = std::clamp(
        int(std::lround(ratio * innerWidth)), 0, innerWidth);
    if (fillWidth > 0) {
        const QRect fill(bar.left() + 2, bar.top() + 2,
                         fillWidth, std::max(0, bar.height() - 4));
        QLinearGradient gradient(fill.topLeft(), fill.topRight());
        gradient.setColorAt(0.0, QColor(43, 125, 233));
        gradient.setColorAt(1.0, QColor(22, 163, 93));
        painter.setBrush(gradient);
        painter.drawRoundedRect(fill, 4, 4);
    }

    painter.setPen(QColor(123, 135, 148));
    for (int i = 0; i <= 4; ++i) {
        const int x = bar.left() + int(std::lround(i * bar.width() / 4.0));
        painter.drawLine(x, bar.top() - 3, x, bar.top());
    }
}
