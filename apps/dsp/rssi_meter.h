#pragma once

#include <QWidget>

class RssiMeter final : public QWidget
{
public:
    explicit RssiMeter(QWidget* parent = nullptr);
    void setReading(double value, double minimum, double maximum);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    double value_ = 0.0;
    double minimum_ = 0.0;
    double maximum_ = 10.0;
};
