#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>

namespace asrtu
{
inline constexpr wchar_t kOwnerName[] = L"Local\\ASRTU_BRIDGE_OWNER_V1";
inline constexpr wchar_t kIqName[] = L"Local\\ASRTU_IQ_BRIDGE_V1";
inline constexpr wchar_t kDopplerName[] = L"Local\\ASRTU_DOPPLER_CONTROL_V1";
inline constexpr std::uint32_t kCapacity = 262144;
inline constexpr std::size_t kBytes = 64 + std::size_t(kCapacity) * 8;

// A named event is an exclusive lifetime token, not a signalling primitive.
// Atomic create-or-open rejects even a second instance on the same thread;
// unlike a semaphore, a crashed owner cannot leave it permanently acquired.
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

	bool start(const wchar_t *ownerName = kOwnerName,
		   const wchar_t *iqName = kIqName)
	{
		if (owner_)
			return true;
		error_.clear();
		owner_ = CreateEventW(nullptr, TRUE, FALSE, ownerName);
		const DWORD status = GetLastError();
		if (!owner_ || status == ERROR_ALREADY_EXISTS) {
			if (owner_)
				CloseHandle(owner_);
			owner_ = nullptr;
			error_ = "Another ASRTU bridge plugin is running. Stop "
				 "it before starting this bridge.";
			return false;
		}
		mapping_ = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr,
					      PAGE_READWRITE, 0, DWORD(kBytes),
					      iqName);
		const bool existing = GetLastError() == ERROR_ALREADY_EXISTS;
		if (!mapping_)
			return fail("Cannot create I/Q shared memory.");
		base_ = static_cast<std::uint8_t *>(
		    MapViewOfFile(mapping_, FILE_MAP_ALL_ACCESS, 0, 0, kBytes));
		if (!base_)
			return fail("Cannot map I/Q shared memory.");
		// Older SDR# builds do not know the ownership token. Never
		// overwrite their ring while their process is alive, even if
		// reception is paused.
		if (existing && get<std::uint32_t>(0) == 0x42514941) {
			const DWORD pid = get<DWORD>(56);
			if (pid) {
				HANDLE process =
				    OpenProcess(SYNCHRONIZE, FALSE, pid);
				const bool alive =
				    process ? WaitForSingleObject(process, 0) !=
						  WAIT_OBJECT_0
					    : GetLastError() !=
						  ERROR_INVALID_PARAMETER;
				if (process)
					CloseHandle(process);
				if (alive)
					return fail(
					    "An existing ASRTU I/Q producer is "
					    "running. Close it first.");
			}
		}
		ownsHeader_ = true;
		InterlockedExchange(
		    reinterpret_cast<volatile LONG *>(base_ + 40), 0);
		put(0, std::uint32_t(0x42514941));
		put(4, std::uint32_t(1));
		put(8, double(48000));
		put(16, kCapacity);
		put(20, std::uint32_t(8));
		// Preserve the monotonic cursor when a decoder keeps the
		// mapping open. This prevents a quick producer handover from
		// looking like old data.
		index_ = existing ? get<std::uint64_t>(24) : 0;
		put(32, double(48000));
		put(56, GetCurrentProcessId());
		InterlockedExchange64(
		    reinterpret_cast<volatile LONG64 *>(base_ + 48), 0);
		InterlockedExchange(
		    reinterpret_cast<volatile LONG *>(base_ + 40), 1);
		return true;
	}

	void stop()
	{
		if (base_) {
			if (ownsHeader_) {
				InterlockedExchange(
				    reinterpret_cast<volatile LONG *>(base_ +
								      40),
				    0);
				put(56, DWORD(0));
			}
			UnmapViewOfFile(base_);
		}
		base_ = nullptr;
		ownsHeader_ = false;
		if (mapping_)
			CloseHandle(mapping_);
		mapping_ = nullptr;
		if (owner_)
			CloseHandle(owner_);
		owner_ = nullptr;
	}

	// Samples are two float32 values (I, Q); no PCM quantization or
	// clipping.
	template <class Stereo> void write(const Stereo *samples, int count)
	{
		static_assert(sizeof(Stereo) == 8,
			      "I/Q samples must contain two float32 values");
		if (!base_ || !ownsHeader_ || count <= 0)
			return;
		const auto total = std::uint64_t(count);
		const auto kept = std::min(total, std::uint64_t(kCapacity));
		const auto begin = index_ + total - kept;
		const auto offset = begin % kCapacity;
		const auto first = std::min(kept, kCapacity - offset);
		const auto *source = reinterpret_cast<const std::uint8_t *>(
		    samples + (total - kept));
		std::memcpy(base_ + 64 + offset * 8, source,
			    std::size_t(first) * 8);
		if (first < kept)
			std::memcpy(base_ + 64, source + first * 8,
				    std::size_t(kept - first) * 8);
		index_ += total;
		LARGE_INTEGER now{};
		QueryPerformanceCounter(&now);
		InterlockedExchange64(
		    reinterpret_cast<volatile LONG64 *>(base_ + 24),
		    LONG64(index_));
		InterlockedExchange64(
		    reinterpret_cast<volatile LONG64 *>(base_ + 48),
		    now.QuadPart);
	}

	const std::string &error() const
	{
		return error_;
	}
	bool active() const
	{
		return ownsHeader_;
	}

      private:
	template <class T> T get(std::size_t offset) const
	{
		T value{};
		std::memcpy(&value, base_ + offset, sizeof(value));
		return value;
	}
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
	HANDLE owner_ = nullptr;
	HANDLE mapping_ = nullptr;
	std::uint8_t *base_ = nullptr;
	std::uint64_t index_ = 0;
	bool ownsHeader_ = false;
	std::string error_;
};

class DopplerReader
{
      public:
	~DopplerReader()
	{
		close();
	}
	void close()
	{
		if (base_)
			UnmapViewOfFile(base_);
		if (mapping_)
			CloseHandle(mapping_);
		base_ = nullptr;
		mapping_ = nullptr;
	}
	bool read(std::int64_t &target, std::int64_t &correction,
		  const wchar_t *name = kDopplerName)
	{
		if (!base_) {
			mapping_ = OpenFileMappingW(FILE_MAP_READ, FALSE, name);
			if (!mapping_)
				return false;
			base_ = static_cast<const std::uint8_t *>(
			    MapViewOfFile(mapping_, FILE_MAP_READ, 0, 0, 64));
			if (!base_) {
				close();
				return false;
			}
		}
		// Offset 36 is a seqlock added without changing the V1 payload
		// layout. Odd or changed sequence => writer in flight; retry on
		// the next tick.
		const auto sequence = load<LONG>(36);
		if (sequence & 1)
			return false;
		const auto magic = load<LONG>(0), version = load<LONG>(4);
		target = load<LONG64>(8);
		correction = load<LONG64>(16);
		const auto timestamp = load<LONG64>(24);
		const auto valid = load<LONG>(32);
		const auto after = load<LONG>(36);
		const auto now =
		    std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::system_clock::now().time_since_epoch())
			.count();
		const bool fresh = timestamp > 0 && timestamp <= now &&
				   now - timestamp <= 3000;
		if (!fresh)
			close(); // Reopen if the tracker exits and is
				 // restarted.
		return sequence == after && magic == 0x504f4441 &&
		       version == 1 && valid != 0 && fresh && target > 0;
	}

      private:
	template <class T> T load(std::size_t offset) const
	{
		const T value =
		    *reinterpret_cast<const volatile T *>(base_ + offset);
		MemoryBarrier();
		return value;
	}
	HANDLE mapping_ = nullptr;
	const std::uint8_t *base_ = nullptr;
};
} // namespace asrtu
