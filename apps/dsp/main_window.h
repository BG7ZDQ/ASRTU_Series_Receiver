#pragma once

#include <QFile>
#include <QElapsedTimer>
#include <QMainWindow>
#include <QByteArray>
#include <memory>

class AsrtuFlowgraph;
class QLabel;
class RssiMeter;
class QTimer;
class QNetworkAccessManager;
class SnrPlot;

class MainWindow final : public QMainWindow
{
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void updateStatus();
    void buildUi();
    void appendLog(const QString& text);
    void setSyncDisplay(bool synced);
    void submitSatnogsFrame(const QByteArray& frame);

    std::unique_ptr<AsrtuFlowgraph> flowgraph_;
    SnrPlot* snr_plot_ = nullptr;
    RssiMeter* rssi_meter_ = nullptr;
    QLabel* snr_label_ = nullptr;
    QLabel* frequency_label_ = nullptr;
    QLabel* sync_label_ = nullptr;
    QTimer* status_timer_ = nullptr;
    QFile log_file_;
    QElapsedTimer snr_log_timer_;
    QString session_directory_;
    QString recording_path_;
    bool rssi_range_ready_ = false;
    double rssi_min_ = 0.0;
    double rssi_max_ = 10.0;
    qint64 last_snr_log_ms_ = -1000;
    QNetworkAccessManager* satnogs_network_ = nullptr;
};
