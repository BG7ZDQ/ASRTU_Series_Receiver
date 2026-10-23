#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace asrtu
{
inline constexpr std::uint32_t kCapacity = 262144;
inline constexpr std::size_t kBytes = 64 + std::size_t(kCapacity) * 8;
inline std::string iqPath()
{
	return "/tmp/asrtu-iq-" + std::to_string(getuid()) + ".v1";
}
inline std::string dopplerPath()
{
	return "/tmp/asrtu-doppler-" + std::to_string(getuid()) + ".v1";
}
inline std::int64_t monotonicNs()
{
	return std::chrono::duration_cast<std::chrono::nanoseconds>(
		   std::chrono::steady_clock::now().time_since_epoch())
	    .count();
}
template <class T> T loadShared(const std::uint8_t *base, std::size_t offset)
{
	return __atomic_load_n(reinterpret_cast<const T *>(base + offset),
			       __ATOMIC_ACQUIRE);
}
template <class T>
void storeShared(std::uint8_t *base, std::size_t offset, T value)
{
	__atomic_store_n(reinterpret_cast<T *>(base + offset), value,
			 __ATOMIC_RELEASE);
}

// Never unlink this file: an existing reader and a new writer must retain the
// same inode. flock is released by the kernel on normal exit and crashes.
class SharedBridge
{
      public:
	SharedBridge() = default;
	SharedBridge(const SharedBridge &) = delete;
	SharedBridge &operator=(const SharedBridge &) = delete;
	~SharedBridge()
	{
		stop();
	}
	bool start(const std::string &path = iqPath())
	{
		if (base_)
			return true;
		error_.clear();
		fd_ = open(path.c_str(),
			   O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
		struct stat info = {};
		if (fd_ < 0 || fstat(fd_, &info) || !S_ISREG(info.st_mode) ||
		    info.st_uid != getuid())
			return fail("Cannot open private ASRTU I/Q mapping.");
		if (flock(fd_, LOCK_EX | LOCK_NB))
			return fail(
			    "Another ASRTU bridge is running. Stop it first.");
		if (ftruncate(fd_, kBytes))
			return fail("Cannot size I/Q mapping.");
		void *view = mmap(nullptr, kBytes, PROT_READ | PROT_WRITE,
				  MAP_SHARED, fd_, 0);
		if (view == MAP_FAILED)
			return fail("Cannot map I/Q samples.");
		base_ = static_cast<std::uint8_t *>(view);
		index_ = loadShared<std::uint32_t>(base_, 0) == 0x42514941
			     ? loadShared<std::uint64_t>(base_, 24)
			     : 0;
		storeShared(base_, 40, std::uint32_t(0));
		put(0, std::uint32_t(0x42514941));
		put(4, std::uint32_t(1));
		put(8, double(48000));
		put(16, kCapacity);
		put(20, std::uint32_t(8));
		put(32, double(48000));
		put(56, std::uint32_t(getpid()));
		storeShared(base_, 24, index_);
		storeShared(base_, 48, std::int64_t(0));
		storeShared(base_, 40, std::uint32_t(1));
		return true;
	}
	void stop()
	{
		if (base_) {
			storeShared(base_, 40, std::uint32_t(0));
			munmap(base_, kBytes);
			base_ = nullptr;
		}
		if (fd_ >= 0)
			::close(fd_);
		fd_ = -1;
	}
	template <class Stereo> void write(const Stereo *samples, int count)
	{
		static_assert(sizeof(Stereo) == 8, "float32 I/Q required");
		if (!base_ || count <= 0)
			return;
		const auto total = std::uint64_t(count);
		const auto kept = std::min(total, std::uint64_t(kCapacity));
		const auto offset = (index_ + total - kept) % kCapacity;
		const auto first = std::min(kept, kCapacity - offset);
		const auto *data = reinterpret_cast<const std::uint8_t *>(
		    samples + total - kept);
		std::memcpy(base_ + 64 + offset * 8, data, first * 8);
		if (first < kept)
			std::memcpy(base_ + 64, data + first * 8,
				    (kept - first) * 8);
		index_ += total;
		storeShared(base_, 24, index_);
		storeShared(base_, 48, monotonicNs());
	}
	const std::string &error() const
	{
		return error_;
	}
	bool active() const
	{
		return base_ != nullptr;
	}

      private:
	template <class T> void put(std::size_t offset, T value)
	{
		std::memcpy(base_ + offset, &value, sizeof(value));
	}
	bool fail(const char *message)
	{
		error_ = message;
		stop();
		return false;
	}
	int fd_ = -1;
	std::uint8_t *base_ = nullptr;
	std::uint64_t index_ = 0;
	std::string error_;
};

// Same 64-byte V1 control record as Windows. TinyDoppler is the only writer;
// SDR++ opens the per-user file read-only and treats a stopped/stale tracker
// as "no Doppler" without touching any rigctl endpoint.
class DopplerReader
{
      public:
	explicit DopplerReader(std::string path = dopplerPath())
	    : path_(std::move(path))
	{
	}
	~DopplerReader()
	{
		close();
	}
	void close()
	{
		if (base_) {
			munmap(const_cast<std::uint8_t *>(base_), 64);
			base_ = nullptr;
		}
		if (fd_ >= 0)
			::close(fd_);
		fd_ = -1;
	}
	bool read(std::int64_t &target, std::int64_t &correction)
	{
		if (!base_) {
			fd_ = open(path_.c_str(),
				   O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
			struct stat info = {};
			if (fd_ < 0 || fstat(fd_, &info) ||
			    !S_ISREG(info.st_mode) || info.st_uid != getuid() ||
			    info.st_size != 64) {
				close();
				return false;
			}
			void *view =
			    mmap(nullptr, 64, PROT_READ, MAP_SHARED, fd_, 0);
			if (view == MAP_FAILED) {
				close();
				return false;
			}
			base_ = static_cast<const std::uint8_t *>(view);
		}
		const auto sequence = loadShared<std::uint32_t>(base_, 36);
		if (sequence & 1)
			return false;
		const auto magic = loadShared<std::uint32_t>(base_, 0);
		const auto version = loadShared<std::uint32_t>(base_, 4);
		target = loadShared<std::int64_t>(base_, 8);
		correction = loadShared<std::int64_t>(base_, 16);
		const auto timestamp = loadShared<std::int64_t>(base_, 24);
		const auto valid = loadShared<std::int32_t>(base_, 32);
		const auto after = loadShared<std::uint32_t>(base_, 36);
		const auto now =
		    std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch())
			.count();
		const bool fresh = timestamp > 0 && timestamp <= now &&
				   now - timestamp <= 3000;
		if (!fresh)
			close();
		return sequence == after && magic == 0x504f4441U &&
		       version == 1 && valid != 0 && fresh && target > 0;
	}

      private:
	std::string path_;
	int fd_ = -1;
	const std::uint8_t *base_ = nullptr;
};
} // namespace asrtu
