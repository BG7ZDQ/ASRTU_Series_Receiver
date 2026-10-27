#pragma once

#include <QVector>
#include <QWidget>
#include "snr_scale.h"
#include "snr_history.h"

class SnrPlot final : public QWidget
{
public:
    explicit SnrPlot(QWidget* parent = nullptr);
    bool addValue(double value, double audioSeconds);
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
  SnrHistory history_;
  SnrScale scale_;
};
