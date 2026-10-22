#include "shared_bridge.h"
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
struct Sample { float i, q; };
struct View {
    HANDLE map;
    std::uint8_t* data;
    View(const wchar_t* name, std::size_t bytes) {
        map = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                                 0, DWORD(bytes), name);
        check(map != nullptr, "create test mapping");
        data = static_cast<std::uint8_t*>(MapViewOfFile(map, FILE_MAP_ALL_ACCESS, 0, 0, bytes));
        check(data != nullptr, "map test memory");
    }
    ~View() { UnmapViewOfFile(data); CloseHandle(map); }
    template<class T> T get(int offset) { T v; std::memcpy(&v, data + offset, sizeof(v)); return v; }
    template<class T> void put(int offset, T v) { std::memcpy(data + offset, &v, sizeof(v)); }
};
}

int wmain(int argc, wchar_t** argv) {
    if (argc == 2 && std::wstring(argv[1]) == L"--probe-default") {
        asrtu::SharedBridge contender;
        return contender.start() ? 1 : 0;
    }
    if (argc == 5 && std::wstring(argv[1]) == L"--hold") {
        asrtu::SharedBridge child;
        if (!child.start(argv[2], argv[3])) return 2;
        HANDLE ready = OpenEventW(EVENT_MODIFY_STATE, FALSE, argv[4]);
        if (!ready) return 3;
        SetEvent(ready);
        CloseHandle(ready);
        Sleep(INFINITE);
        return 0;
    }
    try {
        const std::wstring prefix = L"Local\\ASRTU_TEST_" + std::to_wstring(GetCurrentProcessId());
        const auto owner = prefix + L"_OWNER", iq = prefix + L"_IQ";
        asrtu::SharedBridge first, second;
        check(first.start(owner.c_str(), iq.c_str()), "first producer rejected");
        View view(iq.c_str(), asrtu::kBytes);
        check(view.get<std::uint32_t>(0) == 0x42514941 && view.get<double>(8) == 48000,
              "incompatible I/Q header");
        const Sample sample{1.25f, -0.125f};
        first.write(&sample, 1);
        check(!second.start(owner.c_str(), iq.c_str()), "second producer was allowed");
        check(view.get<std::uint64_t>(24) == 1 && view.get<int>(40) == 1 &&
              view.get<Sample>(64).i == sample.i, "rejected producer changed the live ring");
        std::vector<Sample> burst(asrtu::kCapacity + 17);
        for (std::size_t i = 0; i < burst.size(); ++i) burst[i] = {float(i), -float(i)};
        first.write(burst.data(), int(burst.size()));
        const auto end = view.get<std::uint64_t>(24);
        for (std::uint64_t i = end - asrtu::kCapacity; i < end; ++i) {
            const auto received = view.get<Sample>(int(64 + (i % asrtu::kCapacity) * 8));
            check(received.i == float(i - 1) && received.q == -float(i - 1), "ring wrap corrupted I/Q");
        }
        check(view.get<std::int64_t>(48) > 0, "missing sample heartbeat");
        first.stop();
        check(view.get<int>(40) == 0 && view.get<DWORD>(56) == 0, "stop retained ownership");
        check(second.start(owner.c_str(), iq.c_str()), "stopped producer blocks restart");
        second.write(&sample, 1);
        check(view.get<std::uint64_t>(24) == end + 1, "handover reset cursor");
        second.stop();
        // A legacy producer with no event still owns its mapping while paused.
        view.put(56, GetCurrentProcessId());
        view.put(40, 1);
        check(!first.start(owner.c_str(), iq.c_str()), "legacy producer overwritten");
        check(view.get<int>(40) == 1, "failed legacy probe disabled its producer");
        view.put(56, DWORD(0));

        const auto readyName = prefix + L"_READY";
        HANDLE ready = CreateEventW(nullptr, TRUE, FALSE, readyName.c_str());
        wchar_t executable[MAX_PATH];
        GetModuleFileNameW(nullptr, executable, MAX_PATH);
        std::wstring command = L"\"" + std::wstring(executable) + L"\" --hold " + owner + L" " + iq + L" " + readyName;
        STARTUPINFOW startup{}; startup.cb = sizeof(startup);
        PROCESS_INFORMATION child{};
        check(CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE,
                             CREATE_NO_WINDOW, nullptr, nullptr, &startup, &child), "spawn competing producer");
        const bool childReady = WaitForSingleObject(ready, 5000) == WAIT_OBJECT_0;
        const bool blocked = childReady && !first.start(owner.c_str(), iq.c_str());
        // Crash only the child process created by this test.
        TerminateProcess(child.hProcess, 99);
        WaitForSingleObject(child.hProcess, 5000);
        CloseHandle(child.hThread); CloseHandle(child.hProcess); CloseHandle(ready);
        check(blocked, "cross-process ownership failed");
        check(first.start(owner.c_str(), iq.c_str()), "crashed owner left a stale lock");
        first.stop();

        const auto control = prefix + L"_DOPPLER";
        View commandView(control.c_str(), 64);
        std::memset(commandView.data, 0, 64);
        commandView.put(0, std::uint32_t(0x504f4441));
        commandView.put(4, std::uint32_t(1));
        commandView.put(8, std::int64_t(436210123));
        commandView.put(16, std::int64_t(-123));
        const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        commandView.put(24, std::int64_t(now));
        commandView.put(32, 1);
        commandView.put(36, 2);
        asrtu::DopplerReader reader;
        std::int64_t target{}, correction{};
        check(reader.read(target, correction, control.c_str()) && target == 436210123 && correction == -123,
              "valid Doppler command rejected");
        commandView.put(36, 3);
        check(!reader.read(target, correction, control.c_str()), "torn Doppler update accepted");
        commandView.put(36, 4);
        commandView.put(24, std::int64_t(now - 4000));
        check(!reader.read(target, correction, control.c_str()), "stale Doppler command accepted");
        commandView.put(24, std::int64_t(now + 4000));
        check(!reader.read(target, correction, control.c_str()), "future Doppler command accepted");
        commandView.put(24, std::int64_t(now));
        commandView.put(32, 0);
        check(!reader.read(target, correction, control.c_str()), "disabled Doppler command accepted");
        std::cout << "PASS: float I/Q, ring wrap, contention, handover, legacy protection, crash recovery, Doppler validity\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
