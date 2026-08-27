#pragma once

#include <QByteArray>
#include <QImage>
#include <QString>

#include <cstdint>
#include <functional>
#include <map>

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

    void ingestFrame(const QByteArray& frame);
    void clear();

private:
    bool rebuildImage();

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
};
