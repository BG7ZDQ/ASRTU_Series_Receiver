#include "ssdv_receiver.h"

extern "C" {
#include "ssdv.h"
}

#include <QDateTime>
#include <QDir>
#include <QSaveFile>

#include <chrono>
#include <limits>
#include <vector>

namespace {
constexpr int kCcsdsFrameSize = 223;
constexpr int kCcsdsShortHeaderSize = 5;
constexpr int kDslwpPacketSize = 218;
constexpr std::size_t kImageBufferSize = 32 * 1024 * 1024;
constexpr std::size_t kMaxQueuedFrames = 4096;
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

void SsdvReceiver::ingestFrame(const QByteArray& frame)
{
	{
		std::lock_guard<std::mutex> lock(queue_mutex_);
		if (stop_requested_)
			return;
		if (frame_queue_.size() == kMaxQueuedFrames) {
			frame_queue_.pop_front();
			++dropped_frames_;
		}
		frame_queue_.push_back({ frame, queue_generation_ });
	}
	queue_cv_.notify_one();
}

void SsdvReceiver::clear()
{
	{
		std::lock_guard<std::mutex> lock(queue_mutex_);
		frame_queue_.clear();
		++queue_generation_;
		clear_requested_ = true;
	}
	queue_cv_.notify_one();
}

bool SsdvReceiver::isCurrentGeneration(std::uint64_t generation)
{
	std::lock_guard<std::mutex> lock(queue_mutex_);

	return !stop_requested_ && queue_generation_ == generation;
}

void SsdvReceiver::workerLoop()
{
	bool dirty = false;
	auto nextRefresh = std::chrono::steady_clock::now();

	while (true) {
		std::deque<QueuedFrame> batch;
		std::uint64_t generation;
		std::size_t droppedFrames;
		bool clearNow;

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
			generation = queue_generation_;
			droppedFrames = dropped_frames_;
			dropped_frames_ = 0;
			batch.swap(frame_queue_);
		}

		if (clearNow) {
			clearState(generation);
			dirty = false;
			nextRefresh = std::chrono::steady_clock::now();
		}
		if (droppedFrames != 0 && log_callback_) {
			log_callback_(QStringLiteral("SSDV input queue dropped %1 frames")
					  .arg(droppedFrames));
		}

		bool urgent = false;
		for (const auto& queued : batch) {
			if (!isCurrentGeneration(queued.generation))
				continue;
			if (processFrame(queued.frame, queued.generation)) {
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

bool SsdvReceiver::processFrame(const QByteArray& frame,
				std::uint64_t generation)
{
	if (frame.size() != kCcsdsFrameSize || virtualChannelId(frame) != 1)
		return false;

	const auto spacecraftHeader =
		(std::uint16_t(std::uint8_t(frame[0])) << 8) |
		std::uint8_t(frame[1]);
	QByteArray packet = frame.mid(kCcsdsShortHeaderSize, kDslwpPacketSize);
	int errors = 0;

	if (packet.size() != kDslwpPacketSize)
		return false;

	auto* bytes = reinterpret_cast<std::uint8_t*>(packet.data());
	if (ssdv_dec_is_packet(bytes, &errors, ssdv_dslwp_mode) != SSDV_OK)
		return false;

	ssdv_packet_info_t info{};
	ssdv_dec_header(&info, bytes, ssdv_dslwp_mode);

	const auto existing = packets_.find(info.packet_id);
	const auto firstPacket = packets_.find(0);
	const bool replacedFirstPacket =
		info.packet_id == 0 && firstPacket != packets_.end() &&
		firstPacket->second != packet;
	const bool duplicatePacket =
		existing != packets_.end() && existing->second == packet;
	const bool restartCompletedSession = complete_ &&
		existing == packets_.end();
	const bool sessionChanged = image_id_ != int(info.image_id) ||
		spacecraft_header_ != spacecraftHeader || width_ != info.width ||
		height_ != info.height || quality_ != info.quality ||
		replacedFirstPacket || restartCompletedSession;

	if (sessionChanged) {
		packets_.clear();
		image_id_ = info.image_id;
		width_ = info.width;
		height_ = info.height;
		quality_ = info.quality;
		complete_ = false;
		spacecraft_header_ = spacecraftHeader;
		satellite_ = satelliteName(spacecraft_header_);
		state_generation_ = generation;
		image_path_ = QDir(session_directory_).filePath(
			QStringLiteral("SSDV_%1_Pass%2_ID%3.jpg")
				.arg(QDateTime::currentDateTime().toString(
					QStringLiteral("yyyyMMdd_HHmmss_zzz")))
				.arg(++session_serial_)
				.arg(image_id_));
		if (log_callback_) {
			log_callback_(QStringLiteral("SSDV image started: ID %1, %2x%3, quality %4")
					  .arg(image_id_).arg(width_).arg(height_).arg(quality_));
		}
	}

	if (duplicatePacket && !sessionChanged)
		return false;

	packets_[info.packet_id] = packet;
	complete_ = complete_ || info.eoi != 0;

	return true;
}

bool SsdvReceiver::rebuildImage()
{
	if (packets_.empty() || !isCurrentGeneration(state_generation_))
		return false;

	ssdv_t decoder{};
	if (ssdv_dec_init(&decoder) != SSDV_OK)
		return false;

	std::vector<std::uint8_t> jpegBuffer(kImageBufferSize);
	if (ssdv_dec_set_buffer(&decoder, jpegBuffer.data(), jpegBuffer.size()) !=
	    SSDV_OK)
		return false;

	for (const auto& entry : packets_) {
		const auto* packet = reinterpret_cast<const std::uint8_t*>(
			entry.second.data());
		const char result = ssdv_dec_feed(&decoder, packet, ssdv_dslwp_mode);

		if (result != SSDV_FEED_ME && result != SSDV_OK) {
			if (log_callback_) {
				log_callback_(QStringLiteral("SSDV decoder rejected packet %1")
						  .arg(entry.first));
			}
			return false;
		}
	}

	std::uint8_t* jpeg = nullptr;
	std::size_t jpegLength = 0;
	if (ssdv_dec_get_jpeg(&decoder, &jpeg, &jpegLength) != SSDV_OK ||
	    jpeg == nullptr || jpegLength == 0 ||
	    jpegLength > std::size_t(std::numeric_limits<int>::max()))
		return false;

	const QByteArray encoded(reinterpret_cast<const char*>(jpeg),
				 int(jpegLength));
	const QImage image = QImage::fromData(encoded, "JPEG");
	if (image.isNull() || !isCurrentGeneration(state_generation_))
		return false;

	QSaveFile output(image_path_);
	if (!output.open(QIODevice::WriteOnly) ||
	    output.write(encoded) != encoded.size() || !output.commit()) {
		if (log_callback_) {
			log_callback_(QStringLiteral("Unable to save SSDV image: %1")
					  .arg(image_path_));
		}
		return false;
	}

	if (!isCurrentGeneration(state_generation_))
		return false;

	const int first = packets_.begin()->first;
	const int last = packets_.rbegin()->first;
	std::vector<std::uint16_t> missingIds;

	for (int id = 0; id <= last; ++id) {
		if (packets_.find(std::uint16_t(id)) == packets_.end())
			missingIds.push_back(std::uint16_t(id));
	}

	SsdvImageUpdate update;
	update.image = image;
	update.path = image_path_;
	update.satellite = satellite_;
	update.generation = state_generation_;
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

void SsdvReceiver::clearState(std::uint64_t generation)
{
	packets_.clear();
	image_path_.clear();
	image_id_ = -1;
	satellite_.clear();
	spacecraft_header_ = 0;
	width_ = 0;
	height_ = 0;
	quality_ = 0;
	state_generation_ = generation;
	complete_ = false;
}
