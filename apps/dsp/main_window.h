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
class QLocalServer;
class QLocalSocket;
class SnrPlot;
class SsdvImageWindow;
class SsdvReceiver;

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
    void setupControlServer();
    void handleControlSocket(QLocalSocket* socket);
    void switchAudioDevice(int deviceId);
    void handleDecodedFrame(const QByteArray& frame);
    std::unique_ptr<AsrtuFlowgraph> createFlowgraph(int deviceId,
                                                    const QString& recordingPath);

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
    int iq_mismatch_ticks_ = 0;
    bool iq_mismatch_warned_ = false;
    QNetworkAccessManager* satnogs_network_ = nullptr;
    QLocalServer* control_server_ = nullptr;
    QString playback_path_;
    bool fast_playback_ = false;
    bool real_if_12khz_ = false;
    bool shared_iq_bridge_ = false;
    bool recording_enabled_ = false;
    int audio_device_id_ = -1;
    int recording_segment_ = 1;
    std::unique_ptr<SsdvReceiver> ssdv_receiver_;
    SsdvImageWindow* ssdv_window_ = nullptr;
};
