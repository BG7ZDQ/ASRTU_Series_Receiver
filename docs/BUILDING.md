# 构建说明

## Windows（当前完整支持）

已验证的构建组合：

- Windows 10/11 x64
- Visual Studio 2022 Build Tools（MSVC）
- CMake 3.20+
- Qt 5、Qwt、GNU Radio 3.10
- `gr-lilacsat`（固定到 CI/发行脚本记录的提交）
- radioconda（默认路径为 `C:\ProgramData\radioconda`）

```powershell
.\build_release.ps1
```

可覆盖运行环境和构建目录：

```powershell
.\build_release.ps1 `
  -RuntimeRoot C:\ProgramData\radioconda `
  -BuildDir C:\build\asrtu
```

输出的便携目录位于 `portable/ASRTU1_Demod_CQt`。

Windows CI（`.github/workflows/ci.yml` 的 `windows` job）会在每次 push/PR 时
构建自包含便携包并上传 `asrtu-windows-x64.zip` 产物：解压后即可在未安装
radioconda/GNU Radio 的 Windows 上直接运行（无需安装器）。CI 在上传前会对
打包产物做启动冒烟测试（启动器会话检查与解码器截图），并在 `windows-installer`
job 中把同一份最新便携包组装为 Inno Setup 安装器。

## SDR# 插件

多普勒应用和 SDR# 插件维护在独立的
[Tiny Doppler](https://github.com/BG7ZDQ/TinyDoppler) 仓库中。
首次获取主工程时使用 `git clone --recurse-submodules`；已有检出执行
`git submodule update --init --recursive`。插件面向兼容旧版插件 API 的
SDR#，需要兼容的 SDR# API 程序集：

```powershell
.\external\TinyDoppler\plugin\build_legacy.ps1 `
  -SdrSharpApiRoot C:\path\to\SDRSharp `
  -Configuration Release
```

## Linux

Linux 可以构建 `ASRTU1_Launcher`、`ASRTU1_Demod_CQt`、
`TinyDoppler`、`ASRTU_UploadProxy` 和跨平台的
`ASRTU_SatnogsUploader`。Windows 代理包装器、SDR# 插件和
Inno Setup 安装器不会生成。Linux 原生上传代理会反序列化 GNU Radio PMT
PDU，并拒绝非 223 字节的遥测帧；它不使用 Windows 旧代理的固定头偏移。
建议使用 GNU Radio 3.10、Qt 5 和同一 ABI/编译器构建全部 OOT 模块。

以 Ubuntu/Debian 为例，基础依赖可安装为：

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build pkg-config \
  qtbase5-dev libqt5svg5-dev libqt5opengl5-dev libqt5websockets5-dev \
  libqwt-qt5-dev libasound2-dev libjack-jackd2-dev portaudio19-dev \
  gnuradio-dev libvolk2-dev libfftw3-dev libboost-all-dev \
  libsndfile1-dev libzmq3-dev
```

此外必须先从源码安装与 GNU Radio 3.10 兼容的 `gr-lilacsat`。
若它安装在非系统前缀，请把该前缀加入
`CMAKE_PREFIX_PATH`、`CMAKE_INCLUDE_PATH` 和 `CMAKE_LIBRARY_PATH`。

```bash
cmake -S . -B build-linux -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DASRTU_BUILD_BENCHMARK=OFF
cmake --build build-linux --parallel
```

启动器及录音回放示例：

```bash
./build-linux/ASRTU1_Launcher
./build-linux/ASRTU_UploadProxy
./build-linux/ASRTU_UploadProxy --gui
./build-linux/ASRTU1_Demod_CQt --wav /path/to/stereo_iq.wav --no-record
./build-linux/ASRTU1_Demod_CQt --wav /path/to/mono_12khz_if.wav \
  --real-if-12k --no-record
```

Linux 当前范围与限制：

- 从启动器启动 Linux 上传代理时会显示独立窗口；关闭启动器或接收器不会
  停止上传。关闭代理窗口可手动结束代理。
  直接执行 `ASRTU_UploadProxy` 则仍采用无窗口命令行模式。
- 录音文件、GNU Radio DSP、FEC、Qt图形、TCP/ZMQ输出可作为主要移植路径。
- Linux 默认把录音和日志写入当前用户的 XDG 数据目录，不会写入 AppImage
  挂载点或 `/usr` 等系统安装目录。
- Linux 启动器当前使用系统默认音频输入；数字设备选择会映射为
  ALSA `plughw:<编号>,0`，仍需在目标发行版上核对设备枚举。
- Doppler 窗口可以在 Linux 计算并显示跟踪结果；自动控制 SDR# 的共享内存
  发布仍是 Windows 专用功能。
- Linux 实时声卡使用 GNU Radio 音频后端，需要在目标发行版实测。
- SDR# 本地共享内存桥使用 Windows named mapping；Linux 构建中不提供该
  输入，需要改用声卡/录音，或另行实现跨平台共享传输。
- Linux CI 会执行严格编译、单元测试、Cppcheck、Clang-Tidy、ASan、UBSan
  和 TSan，并构建 AppImage、deb、rpm；Arch Linux 打包元数据由
  `packaging/arch/PKGBUILD` 提供并在 CI 中校验。
- GitHub 的 AppImage、deb、rpm 发行包任务在 Ubuntu 22.04 环境中构建；
  AppImage 还会检查内部 ELF 和最终 AppImage 启动程序的 glibc 要求不高于
  2.35；未知的命名 ABI、私有 ABI 和无法解析的 ELF 都会使检查失败。CI 构建测试
  与发布包构建使用不同 runner，避免新系统的 ABI 混入发布包。
- Linux 发行包属于 CI 产物，正式发布仅由 `v*` tag 触发；运行时硬件和 OOT
  模块兼容性仍需在目标发行版上实测。
- `benchmark_main.cpp` 使用 Windows 进程统计 API，非 Windows 默认关闭
  `ASRTU_BUILD_BENCHMARK`。

## macOS（移植基础，尚未验证）

解码核心使用 C++17、Qt 5 和 GNU Radio，可作为 macOS 移植基础；但当前完整
应用仍包含 WinMM 声卡枚举、Windows 共享内存、SDR# 插件和 Inno Setup 等
Windows 专用部分。macOS 版本需要验证 Core Audio 输入、替代共享内存传输，
并在可用的 GNU Radio 3.10/OOT 模块上重新验证 DSP 与应用打包。当前仓库不
宣称已经提供可直接发布的 macOS 构建。

## 验证建议

### Qt GUI sanitizer 测试

Linux sanitizer 配置使用 `-DTINY_DOPPLER_TEST_QPA_PLATFORM=minimal`。
Qt 5 的 `offscreen` 插件可在仅创建 `QApplication` 的对照程序中报告
368 字节的屏幕初始化泄漏；不要为此关闭 `detect_leaks`。

TinyDoppler 工作流测试注入异步模拟下载和空窗口图标，不初始化真实网络后端，
也不触发发行版 Qt 的 PNG 并行转换线程池。仅加载原窗口图标的 Qt 对照程序
即可复现该线程池的 TSan 报告，因此流程测试不检查这段未插桩的第三方代码。
设置 `TINY_DOPPLER_QA_DIR` 的截图测试仍使用真实图标，应与 sanitizer 分开运行。
此隔离不替代真实联网及界面外观测试，正式程序的网络实现和图标保持不变。

若旧版 TSan 在新内核上启动即报 `unexpected memory mapping`，可在本地用
`setarch x86_64 -R ctest --test-dir build-tsan --output-on-failure` 仅为测试进程
固定地址布局；不要把这种启动失败视为通过，也无需修改全系统的 ASLR 设置。

### Ubuntu 22.04 发行基线

发行任务固定使用 `ubuntu-22.04`，不能直接拿 Ubuntu 24.04 的测试构建来打包。
OOT 库缓存按 Ubuntu 版本隔离。静态分析和 sanitizer 仍可使用较新的 runner，
它们的构建产物不进入发行包。

本地 AppImage 打包后可执行与 CI 相同的检查：

```bash
python3 ci/check_linux_abi.py dist/appimage/AppDir dist/appimage/*.AppImage
```

符号检查只是必要条件，还需在 22.04 上启动实际产物。CI 在 22.04 发行任务中
验证启动器、SatNOGS 窗口和 TinyDoppler 卫星管理窗口；声卡与实际接收另行实测。
历史发行包不会因修改 CI 自动更新，需要重新构建并发布新产物。

### Arch Linux 打包工具

本地生成 rpm 需要 `rpm-tools`，其中包含 `rpmbuild`：

```bash
sudo pacman -S --needed rpm-tools
```

本地生成 AppImage 需要 `linuxdeploy` 和 Qt plugin。可以下载官方
AppImage 版本并赋予执行权限：

```bash
mkdir -p "$HOME/.local/bin"
curl --fail --location --retry 3 \
  -o "$HOME/.local/bin/linuxdeploy.AppImage" \
  https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
curl --fail --location --retry 3 \
  -o "$HOME/.local/bin/linuxdeploy-plugin-qt.AppImage" \
  https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
chmod +x "$HOME/.local/bin/linuxdeploy"*
```

Arch 用户也应安装 `patchelf`、`desktop-file-utils`、`fuse2` 和 `rpm-tools`。
构建 Linux 上传代理还需要 `qt5-websockets`。
AppImage 运行时若系统启用了较新的 FUSE，使用 `APPIMAGE_EXTRACT_AND_RUN=1`
可绕过 FUSE 挂载限制。

每次发布至少验证：程序冷启动、三种实时输入、文件播放、切换/移除声卡、启停录音、FEC 帧输出、代理启动、TLE 下载、多普勒开关、中文/英文/日文界面以及 100%/150%/200% DPI。Linux 或 macOS 构建应在对应系统上另行完成编译、声卡、文件回放和 FEC 回归，不能用 Windows 构建结果代替。
