# Windows 打包

## 安装器输入

`packaging/package_portable.ps1` 从 Release 构建复制程序、Qt 平台插件、GNU Radio/Qt 运行库、翻译 `.qm` 和必要许可证文件。

## 安装器

安装器使用 Inno Setup 6，开发版本号为 1.5.5（尚未正式发布）。全新 Windows 环境只需安装 Visual Studio 2022 Build Tools 和 Inno Setup 6，然后运行：

```powershell
.\build_installer.ps1
```

该默认路径会重新编译 SDR# 桥接插件，从 `packaging/payload/` 创建干净的 staging，并生成 `packaging/inno/dist/ASRTU_Series_Receiver-<版本号>-x86_64-WinInstaller.exe`（例如 `ASRTU_Series_Receiver-1.5.5-x86_64-WinInstaller.exe`，版本号取自 `CMakeLists.txt`）。安装器启动时始终由用户从简体中文、English、日本語中选择安装向导语言。若本地没有 `packaging/payload/sdrpp` 或 `packaging/payload/plutosdr-driver`，脚本会跳过对应可选组件并输出警告；此时生成的是核心安装器（仍包含 SDR# 预设），完整安装器需要本地补齐这两个载荷。

GitHub Actions 的 `windows-installer` job 下载已通过冒烟测试的运行目录来构建安装器，
然后将 EXE 装入 `ASRTU_Series_Receiver-<版本号>-x86_64-WinInstaller.zip`。
不再构建或发布 WinPortable.zip。打包前逐一校验主程序版本与 CMake 一致；
仓库历史 payload 中的 1.5.4 二进制不能冒充 1.5.5，应使用下面的重编命令。

若还要重编 DSP、启动器和代理包装器：

```powershell
.\packaging\inno\build_installer.ps1 -RebuildDsp
```

此路径另外要求安装 radioconda/GNU Radio、Qt、Qwt 和 `gr-lilacsat` 开发环境。

## Linux 安装包

`linux-bundle` CI job 把 AppImage、Linux `asrtu_sink.so`、可选 SDR++ Debian
包和 `packaging/linux/setup.sh` 组装成：

```text
ASRTU_Series_Receiver-<版本号>-x86_64-LinuxAppImage.zip
  setup.sh
  ASRTU_Series_Receiver-<版本号>-x86_64-LinuxAppImage/
    ASRTU_Series_Receiver-<版本号>-x86_64-LinuxAppImage.AppImage
    plugins/asrtu_sink.so
    sdrpp/sdrpp_ubuntu_jammy_amd64.deb
```

`setup.sh` 支持安装语言选择，并询问是否安装 SDR++；不需要 SDR++ 时只安装
ASRTU。SDR++ 包只针对 Ubuntu 22.04/Jammy x86_64，因此该 ZIP 也是 x86_64
专用。脚本不会覆盖系统 Qt/GNU Radio，AppImage 解包安装到用户目录。

## SDR++ 插件

所有平台的插件统一放入 `SDR_Software_Plugins-<版本号>-AllPlatform.zip`，
顶层为 `SDR#` 和 `SDR++`，内部按系统和架构分目录。

`integrations/sdrpp/` 提供 Windows `asrtu_sink` 共享内存插件。SDR++ 的
RAW 模式输出以 48 kHz 浮点 I/Q 写入与 SDR# 相同的共享内存，多普勒指令
也从 TinyDoppler 共享内存读取。启动器使用 `--shared-iq-bridge`，不依赖
UDP 或 Rigctl。新版 SDR# 插件与 SDR++ 插件使用同一所有权锁，后启动的
桥接实例会被拒绝，不能发送 I/Q 或调频。详细配置和构建步骤见
[SDR++ 插件说明](../integrations/sdrpp/README.md)。

插件须针对目标 SDR++ 的头文件、核心导入库与架构编译。附带 SDR++ 的本地
便携包应包含 `sdrpp/modules/asrtu_sink.dll`，并将 Radio sink 设置为
`ASRTU Receiver`；同时更新 TinyDoppler 和 SDR# 插件。1.5.5 的 CI 从源码生成 Windows x64、Linux x86_64/arm64、macOS x86_64/arm64
插件，Linux/macOS 提供 POSIX 共享内存输入；详见 [RELEASE_LAYOUT.md](RELEASE_LAYOUT.md)。

Windows 安装包同时包含 PlutoSDR-M2K 官方 USB 驱动文件及其管理员安装入口，
以及 libiio 0.26 的 USB/IP 后端。SDR++ 的 Pluto 模块支持手动输入完整
libiio URI（例如 `ip:192.168.1.10`）。SDR++ 与 SDR# 共用 rtl-sdr Blog 1.4.0
驱动，以支持 RTL-SDR Blog V4 / R828D。

## 第三方再分发

`packaging/payload/` 中的第三方运行组件不适用本项目 MIT 许可，仍须保留各自条款、版权声明和安装器内的第三方告知。
