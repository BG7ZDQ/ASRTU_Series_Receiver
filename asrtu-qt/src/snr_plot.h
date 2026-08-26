#pragma once

#include <QVector>
#include <QWidget>

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
    static constexpr int kMaxPoints = 300; // 200 ms * 300 = 60 s
};
