#pragma once

#include <QByteArray>
#include <QImage>
#include <QString>

#include <cstdint>
#include <functional>
#include <map>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

struct SsdvImageUpdate {
    QImage image;
    QString path;
    QString satellite;
    std::uint16_t spacecraft_header = 0;
    int image_id = -1;
    int width = 0;
    int height = 0;
    int quality = 0;
    int received_packets = 0;
    int first_packet = -1;
    int last_packet = -1;
    int missing_packets = 0;
    // Exact packet IDs received for this image. The UI renders packet
    // reception rather than inferred MCU coverage: before EOI the final
    // packet count is unknown, so only holes in the observed sequence are
    // genuine losses.
    std::vector<std::uint16_t> received_packet_ids;
    // Packet IDs in [first_packet, last_packet] that have not been received;
    // lets the UI render confirmed gaps without guessing from JPEG MCU spans.
    std::vector<std::uint16_t> missing_packet_ids;
    bool complete = false;
};

class SsdvReceiver final
{
public:
    using ImageCallback = std::function<void(const SsdvImageUpdate&)>;
    using LogCallback = std::function<void(const QString&)>;

    SsdvReceiver(QString sessionDirectory,
                 ImageCallback imageCallback,
                 LogCallback logCallback = {});
    ~SsdvReceiver();

    void ingestFrame(const QByteArray& frame);
    void clear();

private:
    void workerLoop();
    bool processFrame(const QByteArray& frame);
    bool rebuildImage();
    void clearState();

    QString session_directory_;
    QString image_path_;
    QString satellite_;
    std::uint16_t spacecraft_header_ = 0;
    ImageCallback image_callback_;
    LogCallback log_callback_;
    std::map<std::uint16_t, QByteArray> packets_;
    int image_id_ = -1;
    int width_ = 0;
    int height_ = 0;
    int quality_ = 0;
    bool complete_ = false;

    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::deque<QByteArray> frame_queue_;
    bool clear_requested_ = false;
    bool stop_requested_ = false;
    std::thread worker_;
};
