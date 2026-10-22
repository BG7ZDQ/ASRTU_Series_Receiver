#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
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
		struct stat info{};
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

// Unix tuning keeps the existing SDR++ Rigctl Server path; this plugin only
// adds the missing lossless I/Q path. Windows retains its V1 control mapping.
class DopplerReader
{
      public:
	void close()
	{
	}
	bool read(std::int64_t &, std::int64_t &)
	{
		return false;
	}
};
} // namespace asrtu
