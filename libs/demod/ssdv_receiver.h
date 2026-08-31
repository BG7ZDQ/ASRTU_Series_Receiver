#pragma once

#include <QByteArray>
#include <QImage>
#include <QString>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

struct SsdvImageUpdate {
	QImage image;
	QString path;
	QString satellite;
	std::uint64_t generation = 0;
	std::uint16_t spacecraft_header = 0;
	int image_id = -1;
	int width = 0;
	int height = 0;
	int quality = 0;
	int received_packets = 0;
	int first_packet = -1;
	int last_packet = -1;
	int missing_packets = 0;
	int crc_failed_packets = 0;
	std::vector<std::uint16_t> received_packet_ids;
	std::vector<std::uint16_t> crc_failed_packet_ids;
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
	struct QueuedFrame {
		QByteArray frame;
		std::uint64_t generation = 0;
	};

	void workerLoop();
	bool processFrame(const QByteArray& frame, std::uint64_t generation);
	bool rebuildImage();
	void clearState(std::uint64_t generation);
	bool isCurrentGeneration(std::uint64_t generation);

	QString session_directory_;
	QString image_path_;
	QString satellite_;
	std::uint16_t spacecraft_header_ = 0;
	ImageCallback image_callback_;
	LogCallback log_callback_;
	std::map<std::uint16_t, QByteArray> packets_;
	std::set<std::uint16_t> crc_failed_packet_ids_;
	int image_id_ = -1;
	int width_ = 0;
	int height_ = 0;
	int quality_ = 0;
	std::uint64_t state_generation_ = 0;
	std::uint64_t session_serial_ = 0;
	bool complete_ = false;
	bool jamx_mode_ = false;
	std::vector<std::uint8_t> jpeg_buffer_;

	std::mutex queue_mutex_;
	std::condition_variable queue_cv_;
	std::deque<QueuedFrame> frame_queue_;
	std::uint64_t queue_generation_ = 0;
	std::size_t dropped_frames_ = 0;
	bool clear_requested_ = false;
	bool stop_requested_ = false;
	std::thread worker_;
};
