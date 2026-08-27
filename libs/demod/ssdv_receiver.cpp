#include "ssdv_receiver.h"

extern "C" {
#include "ssdv.h"
}

#include <QDateTime>
#include <QDir>
#include <QSaveFile>

#include <chrono>
#include <vector>

namespace {
constexpr int kCcsdsFrameSize = 223;
constexpr int kCcsdsShortHeaderSize = 5;
constexpr int kDslwpPacketSize = 218;
constexpr int kImageBufferSize = 4 * 1024 * 1024;
constexpr auto kPreviewRefreshInterval = std::chrono::milliseconds(100);

int virtualChannelId(const QByteArray& frame)
{
    const auto word = (std::uint16_t(std::uint8_t(frame[0])) << 8) |
                      std::uint8_t(frame[1]);
    return (word >> 1) & 0x07;
}

QString satelliteName(std::uint16_t header)
{
    if (header == 0x0322)
        return QStringLiteral("ASRTU-1");
    if (header == 0x2052)
        return QStringLiteral("BY-04");
    return QStringLiteral("Unknown (header %1)")
        .arg(header, 4, 16, QLatin1Char('0')).toUpper();
}
}

SsdvReceiver::SsdvReceiver(QString sessionDirectory,
                           ImageCallback imageCallback,
                           LogCallback logCallback)
    : session_directory_(std::move(sessionDirectory)),
      image_callback_(std::move(imageCallback)),
      log_callback_(std::move(logCallback))
{
    worker_ = std::thread([this] { workerLoop(); });
}

void SsdvReceiver::ingestFrame(const QByteArray& frame)
{
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (stop_requested_)
            return;
        frame_queue_.push_back(frame);
    }
    queue_cv_.notify_one();
}

SsdvReceiver::~SsdvReceiver()
{
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        stop_requested_ = true;
        frame_queue_.clear();
    }
    queue_cv_.notify_all();
    if (worker_.joinable())
        worker_.join();
}

void SsdvReceiver::workerLoop()
{
    bool dirty = false;
    auto nextRefresh = std::chrono::steady_clock::now();
    while (true) {
        std::deque<QByteArray> batch;
        bool clearNow = false;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (!dirty) {
                queue_cv_.wait(lock, [this] {
                    return stop_requested_ || clear_requested_ ||
                           !frame_queue_.empty();
                });
            } else {
                queue_cv_.wait_until(lock, nextRefresh, [this] {
                    return stop_requested_ || clear_requested_ ||
                           !frame_queue_.empty();
                });
            }
            if (stop_requested_)
                break;
            clearNow = clear_requested_;
            clear_requested_ = false;
            batch.swap(frame_queue_);
        }

        if (clearNow) {
            clearState();
            dirty = false;
            nextRefresh = std::chrono::steady_clock::now();
        }
        bool urgent = false;
        for (const auto& frame : batch) {
            if (processFrame(frame)) {
                dirty = true;
                urgent = urgent || complete_;
            }
        }
        const auto now = std::chrono::steady_clock::now();
        if (dirty && (urgent || now >= nextRefresh)) {
            rebuildImage();
            dirty = false;
            nextRefresh = std::chrono::steady_clock::now() +
                          kPreviewRefreshInterval;
        }
    }
}

bool SsdvReceiver::processFrame(const QByteArray& frame)
{
    if (frame.size() != kCcsdsFrameSize || virtualChannelId(frame) != 1)
        return false;

    const auto spacecraftHeader =
        (std::uint16_t(std::uint8_t(frame[0])) << 8) | std::uint8_t(frame[1]);

    QByteArray packet = frame.mid(kCcsdsShortHeaderSize, kDslwpPacketSize);
    if (packet.size() != kDslwpPacketSize)
        return false;

    int errors = 0;
    auto* bytes = reinterpret_cast<std::uint8_t*>(packet.data());
    if (ssdv_dec_is_packet(bytes, &errors, ssdv_dslwp_mode) != 0)
        return false;

    ssdv_packet_info_t info{};
    ssdv_dec_header(&info, bytes, ssdv_dslwp_mode);
    if (image_id_ != int(info.image_id)) {
        packets_.clear();
        image_id_ = info.image_id;
        width_ = info.width;
        height_ = info.height;
        quality_ = info.quality;
        complete_ = false;
        spacecraft_header_ = spacecraftHeader;
        satellite_ = satelliteName(spacecraft_header_);
        image_path_ = QDir(session_directory_).filePath(
            QStringLiteral("SSDV_%1_ID%2.jpg")
                .arg(QDateTime::currentDateTime().toString(
                    QStringLiteral("yyyyMMdd_HHmmss")))
                .arg(image_id_));
        if (log_callback_)
            log_callback_(QStringLiteral("SSDV image started: ID %1, %2x%3, quality %4")
                              .arg(image_id_).arg(width_).arg(height_).arg(quality_));
    }

    const std::uint16_t packetId = info.packet_id;
    const auto existing = packets_.find(packetId);
    if (existing != packets_.end() && existing->second == packet)
        return false;
    packets_[packetId] = packet;
    complete_ = complete_ || info.eoi != 0;
    return true;
}

bool SsdvReceiver::rebuildImage()
{
    if (packets_.empty())
        return false;

    ssdv_t decoder{};
    if (ssdv_dec_init(&decoder) != SSDV_OK)
        return false;
    std::vector<std::uint8_t> jpegBuffer(kImageBufferSize);
    if (ssdv_dec_set_buffer(&decoder, jpegBuffer.data(), jpegBuffer.size()) != SSDV_OK)
        return false;

    for (auto& entry : packets_) {
        auto* packet = reinterpret_cast<std::uint8_t*>(entry.second.data());
        // Starting mid-image is a normal weak-signal case. The reference
        // decoder reports the leading gap but still fills the missing MCUs
        // and can produce the best available partial JPEG.
        ssdv_dec_feed(&decoder, packet, ssdv_dslwp_mode);
    }

    std::uint8_t* jpeg = nullptr;
    std::size_t jpegLength = 0;
    if (ssdv_dec_get_jpeg(&decoder, &jpeg, &jpegLength) != SSDV_OK ||
        jpeg == nullptr || jpegLength == 0)
        return false;

    const QByteArray encoded(reinterpret_cast<const char*>(jpeg), int(jpegLength));
    const QImage image = QImage::fromData(encoded, "JPEG");
    if (image.isNull())
        return false;

    QSaveFile output(image_path_);
    if (!output.open(QIODevice::WriteOnly) || output.write(encoded) != encoded.size() ||
        !output.commit()) {
        if (log_callback_)
            log_callback_(QStringLiteral("Unable to save SSDV image: %1")
                              .arg(image_path_));
        return false;
    }

    const int first = packets_.begin()->first;
    const int last = packets_.rbegin()->first;
    std::vector<std::uint16_t> missingIds;
    // SSDV packet numbering starts at zero. If reception begins in the middle
    // of an image, the unseen leading packet IDs are confirmed losses too.
    for (int id = 0; id <= last; ++id) {
        if (packets_.find(std::uint16_t(id)) == packets_.end())
            missingIds.push_back(std::uint16_t(id));
    }

    SsdvImageUpdate update;
    update.image = image;
    update.path = image_path_;
    update.satellite = satellite_;
    update.spacecraft_header = spacecraft_header_;
    update.image_id = image_id_;
    update.width = width_;
    update.height = height_;
    update.quality = quality_;
    update.received_packets = int(packets_.size());
    update.first_packet = first;
    update.last_packet = last;
    update.missing_packets = int(missingIds.size());
    update.received_packet_ids.reserve(packets_.size());
    for (const auto& entry : packets_)
        update.received_packet_ids.push_back(entry.first);
    update.missing_packet_ids = std::move(missingIds);
    update.complete = complete_;
    if (image_callback_)
        image_callback_(update);
    return true;
}

void SsdvReceiver::clear()
{
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        frame_queue_.clear();
        clear_requested_ = true;
    }
    queue_cv_.notify_one();
}

void SsdvReceiver::clearState()
{
    packets_.clear();
    image_path_.clear();
    image_id_ = -1;
    satellite_.clear();
    spacecraft_header_ = 0;
    width_ = 0;
    height_ = 0;
    quality_ = 0;
    complete_ = false;
}
