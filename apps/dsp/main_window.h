#pragma once

#include <QFile>
#include <QElapsedTimer>
#include <QMainWindow>
#include <QByteArray>
#include <cstdint>
#include <memory>

class AsrtuFlowgraph;
class QLabel;
class RssiMeter;
class QTimer;
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
    void setupControlServer();
    void handleControlSocket(QLocalSocket* socket);
    void switchAudioDevice(int deviceId, bool forceRestart = false);
    void finishAudioDeviceSwitch(AsrtuFlowgraph* stoppedFlowgraph,
                                 int deviceId, int oldDeviceId,
                                 const QString& stopError);
    std::unique_ptr<AsrtuFlowgraph> createFlowgraph(int deviceId,
                                                    const QString& recordingPath);

    std::unique_ptr<AsrtuFlowgraph> flowgraph_;
    AsrtuFlowgraph* unsafe_flowgraph_ = nullptr;
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
    QLocalServer* control_server_ = nullptr;
    QString playback_path_;
    bool fast_playback_ = false;
    bool real_if_12khz_ = false;
    bool shared_iq_bridge_ = false;
    bool recording_enabled_ = false;
    bool audio_switch_in_progress_ = false;
    bool closing_ = false;
    int input_inactive_ticks_ = 0;
    int input_active_ticks_ = 0;
    int audio_restart_attempts_ = 0;
    std::uint64_t last_input_drops_ = 0;
    std::uint64_t last_recording_drops_ = 0;
    bool recording_failure_reported_ = false;
    int audio_device_id_ = -1;
    int pending_audio_device_id_ = -1;
    int recording_segment_ = 1;
    std::unique_ptr<SsdvReceiver> ssdv_receiver_;
    SsdvImageWindow* ssdv_window_ = nullptr;
};
