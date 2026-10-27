#pragma once

#include <QVector>
#include <QWidget>
#include "snr_scale.h"

class SnrPlot final : public QWidget
{
public:
    explicit SnrPlot(QWidget* parent = nullptr);
    void addValue(double value);
    void addGap();
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QVector<double> values_;
    SnrScale scale_;
    static constexpr int kMaxPoints =
	600; // 100 ms * 600 = 60 s of active input
};
