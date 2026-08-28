# 构建说明

## Windows（当前完整支持）

已验证的构建组合：

- Windows 10/11 x64
- Visual Studio 2022 Build Tools（MSVC）
- CMake 3.20+
- Qt 5、Qwt、GNU Radio 3.10
- `gr-lilacsat`、`gr-hyacinth`（兼容 ABI 名称仍为 `hyacinthsat`）
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

## SDR# 插件

插件面向兼容旧版插件 API 的 SDR#，需要合法取得的 SDR# API 程序集：

```powershell
.\plugins\sdrsharp-bridge\build_legacy.ps1 `
  -SdrSharpApiRoot C:\path\to\SDRSharp `
  -Configuration Release
```

## Linux（解码器源码可构建，尚未提供正式发行包）

Linux 可以构建 `ASRTU1_Demod_CQt` 解码器；Windows 启动器、代理包装器、
SDR# 插件和 Inno Setup 安装器不会生成。建议使用 GNU Radio 3.10、Qt 5 和
同一 ABI/编译器构建全部 OOT 模块。

以 Ubuntu/Debian 为例，基础依赖可安装为：

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build pkg-config \
  qtbase5-dev libqt5svg5-dev libqwt-qt5-dev \
  gnuradio-dev libvolk2-dev libfftw3-dev libboost-all-dev \
  libsndfile1-dev libzmq3-dev
```

此外必须先从源码安装与 GNU Radio 3.10 兼容的 `gr-lilacsat` 和
`gr-hyacinth`。若它们安装在非系统前缀，请把该前缀加入
`CMAKE_PREFIX_PATH`、`CMAKE_INCLUDE_PATH` 和 `CMAKE_LIBRARY_PATH`。

```bash
cmake -S . -B build-linux -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DASRTU_BUILD_BENCHMARK=OFF
cmake --build build-linux --parallel
```

启动录音回放示例：

```bash
./build-linux/ASRTU1_Demod_CQt --wav /path/to/stereo_iq.wav --no-record
./build-linux/ASRTU1_Demod_CQt --wav /path/to/mono_12khz_if.wav \
  --real-if-12k --no-record
```

Linux 当前范围与限制：

- 录音文件、GNU Radio DSP、FEC、Qt图形、TCP/ZMQ输出可作为主要移植路径。
- 实时声卡能否工作取决于本机 GNU Radio 音频后端和 `gr-hyacinth` 的
  `stereo_iq_source` 实现，需要在目标发行版实测。
- SDR# 本地共享内存桥使用 Windows named mapping；Linux 构建中该输入不会
  产生样本，需要改用声卡/录音，或另行实现跨平台共享传输。
- 当前没有 Linux 启动器、代理包装器、AppImage/Flatpak/deb/rpm 包，也没有
  Linux CI；因此暂不把 Linux 标记为完整支持的平台。
- `benchmark_main.cpp` 使用 Windows 进程统计 API，非 Windows 默认关闭
  `ASRTU_BUILD_BENCHMARK`。

## macOS（移植基础，尚未验证）

解码核心使用 C++17、Qt 5 和 GNU Radio，可作为 macOS 移植基础；但当前完整
应用仍包含 WinMM 声卡枚举、Windows 共享内存、SDR# 插件和 Inno Setup 等
Windows 专用部分。macOS 版本需要验证 Core Audio 输入、替代共享内存传输，
并在可用的 GNU Radio 3.10/OOT 模块上重新验证 DSP 与应用打包。当前仓库不
宣称已经提供可直接发布的 macOS 构建。

## 验证建议

每次发布至少验证：程序冷启动、三种实时输入、文件播放、切换/移除声卡、启停录音、FEC 帧输出、代理启动、TLE 下载、多普勒开关、中文/英文/日文界面以及 100%/150%/200% DPI。Linux 或 macOS 构建应在对应系统上另行完成编译、声卡、文件回放和 FEC 回归，不能用 Windows 构建结果代替。
