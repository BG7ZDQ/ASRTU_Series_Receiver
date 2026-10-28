# ASRTU Series Satellite Receiver

ASRTU 系列卫星接收、解调、遥测转发与自动多普勒工具。项目包含 C++/Qt 接收解码器、桌面启动器、遥测代理启动包装器，以及 SDR# 本地 RAW I/Q 桥接插件。

Contributors: **BG7ZDQ** **BI1NWO** **BG6HNY** **BG4QBF**

Languages: **中文** · [English](README_EN.md) · [日本語](README_JA.md)

## 主要功能

- 本地共享内存 RAW I/Q、立体声零中频 I/Q、单声道 12 kHz 实数中频和录音文件输入
- BPSK 解调、卷积码/Viterbi 与 FEC 帧输出
- 实时瀑布图、频谱、星座图、SNR、RSSI、环路频偏及同步状态
- 可选 WAV 自动录制，完整帧、符号 SNR 和运行日志按会话保存
- 多 TLE 源下载、卫星选择、频率预设、SGP4 跟踪和 SDR# 自动多普勒控制
- 遥测上传代理及地面站呼号、经纬度配置
- 独立的跨平台 SatNOGS 上传窗口，可查看帧内容、站点信息和服务器反馈（录音回放不会上传）

SNR 无需收到有效数据包：锁定时以 `SNR (Es/N0)` 显示同步 I/Q 功率法得到的符号 Es/N0，失锁且仍有输入时显示 `SNR (SVR)`。断流时冻结数值和曲线，恢复输入后继续；算法暂时无有效结果时保留上一有效读数及其算法标签，首次有效读数前显示 `--`。后台每 100 毫秒音频保存一个 SNR 读数，倍速回放不降低取点密度。历史图显示最近 60 秒音频，快速回放时随实际处理进度推进，并始终保留 0 dB 和整数刻度。估计方法与限制见 [DSP 说明](docs/DSP.md)。

## 使用教程

### 1. 安装与首次设置

1. Windows：解压 `ASRTU_Series_Receiver-<版本号>-x86_64-WinInstaller.zip`，运行其中同名 EXE 安装器，按需求选择是否安装随包提供的 SDR# 遥测预设。
   Linux：解压 `ASRTU_Series_Receiver-<版本号>-x86_64-LinuxAppImage.zip`，进入解压目录执行 `bash setup.sh`，选择安装语言以及是否安装随包的 SDR++。详细步骤见 [Linux 安装说明](docs/LINUX_INSTALL.md)。
   macOS：按机器选择 `ASRTU_Series_Receiver-<版本号>-x86_64-macOS.zip`（Intel）或 `ASRTU_Series_Receiver-<版本号>-arm64-macOS.zip`（Apple Silicon），解压后使用其中的 `.app`。支持范围见 [macOS 说明](docs/MACOS_STATUS.md)。
   独立 SDR 插件：使用 `SDR_Software_Plugins-<版本号>-AllPlatform.zip`，从 `SDR#` 或 `SDR++` 下选择对应系统和架构。
2. 从桌面或应用菜单打开 **ASRTU 系列卫星启动器**。
3. 填写呼号、经度、纬度和海拔。坐标用于卫星跟踪与多普勒计算；启动器会在本机保存设置。
4. 根据接收链路选择“输入”。只有声卡输入模式会显示“声卡”下拉框。

### 2. 选择接收输入

| 输入模式 | 适用场景 | 信号要求 |
| --- | --- | --- |
| 本地共享内存 RAW 模式 I/Q 桥接 | 使用安装包内的任一 SDR 软件及其 ASRTU 插件 | 48 kHz 浮点复数 I/Q，共享内存与自动多普勒控制 |
| 立体声零中频 RAW 模式 I/Q 输入 | SDR 软件通过虚拟声卡输出 I/Q | 左右声道分别为 I、Q，中心频率为 0 Hz |
| 单声道实数域 12 kHz 电台 IF 输入 | 电台、接收机或录音设备输出实数中频 | 目标信号以 +12 kHz 为中心 |

选择声卡模式时，在“声卡”中选择实际输入设备。运行中更换声卡，应先停止当前接收，再选择设备并重新启动。

### 3. 使用共享内存 RAW I/Q 桥接

SDR# 与 SDR++ 使用同一个启动器输入选项和同一块共享内存。每次只启动其中一个桥接，
不要同时启用两边的 ASRTU 插件。

1. 点击 **打开 SDR# 遥测预设**。
2. 在 SDR# 中选择接收设备、设置卫星下行频率并开始播放。
3. 在 ASRTU 插件面板启用本地 RAW I/Q 桥接；需要自动修正频率时，再启用自动多普勒。
4. 回到启动器，选择 **本地内存共享 RAW 模式 I/Q 桥接**，点击 **启动接收**。

如果 SDR# 尚未开始输出样本，解码器会保持 `UNLOCKED`。暂停或关闭 SDR# 后，旧的 SNR 显示不应被当作仍在接收信号。

### SDR++ 共享内存桥接（Windows）

在 SDR++ 中选择 RAW 模式、48 kHz 带宽，并在 Sinks 中选择 **ASRTU Receiver**。
启动器选择 **本地共享内存 RAW 模式 I/Q 桥接** 后启动接收。
打开 TinyDoppler，并在 SDR++ 插件中勾选 **Automatic Doppler tuning** 即可
通过共享内存自动调频。音量和静音不会改变传输的 I/Q。

安装包包含 PlutoSDR-M2K Windows 驱动入口。网络版 PlutoSDR 可在 SDR++ 的
**PlutoSDR Source → IIO URI** 中填写 `ip:192.168.1.10` 这类地址，无需依赖
自动发现。随包的 RTL-SDR Blog 1.4.0 驱动同时用于 SDR++ 和 SDR#，支持
RTL-SDR Blog V4 / R828D。

SDR++ 与新版 SDR# 插件共用互斥保护；已有桥接插件启用时，第二个实例会提示
占用并拒绝启动。先停止原桥接，再在另一边重试。详细步骤见
[SDR++ 插件说明](integrations/sdrpp/README.md)。

### 4. 使用声卡输入

立体声 I/Q 输入应选择能够提供双声道、零中频 RAW 数据的设备；普通扬声器音频不是 I/Q。电台实数中频应选择单声道模式，并保证目标信号位于音频频谱的 12 kHz 附近。解码器的实数输入图会把 12 kHz 显示为 0 Hz，以便直接观察频偏。

选好模式和声卡后点击 **启动接收**。如需保留本次原始输入，先勾选 **自动保存本次接收录音**。

### 5. 判断是否正确接收

- 输入瀑布图中应能看到持续的目标信号，而不是只有宽带噪声。
- BPSK 星座点应逐渐聚集到 I 轴左右两侧。
- `SNR` 上升且环路频偏逐渐稳定时，载波和时钟环通常已进入有效工作区。
- 状态显示 `LOCKED` 表示风信子解码器的载波和定时环路达到锁定判据；`UNLOCKED` 表示未锁定或输入已中断。该指示不依赖 FEC 帧，也不代表解码成功。
- 标题栏显示本次解码窗口累计收到的有效包数，双解码器的重复结果只计一次；切换音频设备或自动重启输入不清零，重新打开窗口后从 0 开始。
- 日志中的 `FEC frame` 和完整十六进制 PDU 可用于确认实际解码；`Symbol Es/N0` 是信号质量估计，不能只看 SNR 曲线判断解码成功。

### 6. 遥测上传

1. 确认呼号和地面站坐标填写正确。
2. 点击 **启动上传代理**，在弹出的列表中选择本次要上传的卫星。
3. 有 MMT 服务器的卫星继续使用原代理；JAMX 没有 MMT 服务器，因此 SatNOGS
   会被强制启用，并启动新的跨平台 `ASRTU_SatnogsUploader` 窗口。
4. SatNOGS 窗口会安全解析 ZMQ PMT PDU，只接受 223 字节遥测帧；左上显示
   十六进制帧内容，右侧显示站点信息，下方显示 HTTPS 服务器反馈。
5. 启动接收与启动上传互相独立；录音回放不会向 ZMQ 发布帧，也不会上传。

### 7. 卫星跟踪与自动多普勒

点击 **卫星跟踪与自动多普勒** 打开 Tiny Doppler。可管理卫星的 NORAD 编号、名称及多组频率，添加或调整星历来源（支持 TLE 与 GP/OMM JSON）；设置保存在安装主目录（Windows）或系统配置目录（Linux/macOS）。确认方位角、仰角、距离和星历日期合理后，再在 SDR# 插件中开启自动多普勒。

自动多普勒只负责修正接收频率，不代表已经成功解码；最终仍以 FEC 帧和日志为准。

### 8. 录音与回放

- 勾选自动录音后，每次接收会在 `ASRTU1_Records/<时间>/` 中保存 WAV 和日志。Windows 默认使用安装目录旁的文件夹，Linux 使用当前用户的数据目录。
- 点击 **打开录音与日志** 可直接进入记录目录。
- **播放录音文件** 用于选择任意受支持的录音；单声道文件按 12 kHz 实数中频处理，双声道文件按零中频 I/Q 处理。非 WAV 格式需要 `sndfile-convert`：Windows 安装包已自带，Linux 需安装 `sndfile-tools`，否则对话框只提供 WAV。
- **快速播放录音文件** 提供更直接的文件选择与回归检查入口，不会改用实时声卡。

### 9. 常见问题

- **一直 UNLOCKED：** 检查卫星是否过境、频率/多普勒是否正确、输入模式是否匹配，以及瀑布图中是否确实有信号。
- **频谱出现镜像或混叠：** 检查是否把单声道实数中频误选为立体声 I/Q，或左右 I/Q 声道是否接反。
- **启动后没有样本：** 对共享内存模式检查 SDR# 是否正在播放且插件桥接已开启；对声卡模式重新选择当前存在的输入设备。
- **有 SNR 但没有帧：** 查看星座、环路频偏和 FEC 日志；载波存在并不等于帧同步和纠错成功。
- **不希望上传：** 只点击“启动接收”，不要启动上传代理即可。

## 目录

- `apps/` — DSP 接收器、启动器与代理包装器
- `libs/` — 解调/FEC 数据链和公共代码
- `external/TinyDoppler/` — 独立维护的 [Tiny Doppler](https://github.com/BG7ZDQ/TinyDoppler) 子模块，含跟踪应用、SDR# 插件及接口文档；源码检出需初始化子模块
- `assets/` — 品牌图、程序图标、界面资源与翻译
- `third_party/` — 保留上游许可的第三方源码
- `packaging/inno/` — Inno Setup 安装脚本
- `packaging/payload/` — 生成完整安装包所需的集成运行组件
- `tests/` — DSP 基准和回归测试入口
- `tools/` — translation and asset maintenance scripts
- `docs/` — architecture, build, translation and release documentation

## 构建

当前改进属于 **1.5.5 开发版**，尚未正式发布。安装 Visual Studio 2022 Build Tools、
Inno Setup 6 和 radioconda/GNU Radio 开发依赖后，从源码构建安装器：

```powershell
.\packaging\inno\build_installer.ps1 -RebuildDsp
```

打包脚本拒绝将旧版 payload 标为新版本。Windows 不再构建便携发布 ZIP。
[电台控频](docs/RADIO_CONTROL.md)通过 Hamlib 接入；
[macOS](docs/MACOS_STATUS.md)与跨平台 SDR++ 插件正在 1.5.5 中验证。

完整环境、Linux/macOS 现状及依赖说明见 [docs/BUILDING.md](docs/BUILDING.md)。安装包构建见 [docs/PACKAGING.md](docs/PACKAGING.md)。

## 文档

- [架构与数据流](docs/ARCHITECTURE.md)
- [构建说明](docs/BUILDING.md)
- [Linux 安装说明](docs/LINUX_INSTALL.md)
- [翻译维护](docs/TRANSLATIONS.md)
- [打包与发布](docs/PACKAGING.md)
- [开源发布检查表](docs/OPEN_SOURCE_CHECKLIST.md)

## 开源与第三方组件

本项目由 BG7ZDQ 以 [MIT License](LICENSE) 开源。第三方组件仍分别适用各自上游许可证，详见 [THIRD_PARTY.md](THIRD_PARTY.md)；MIT 许可不替代、修改或扩大任何第三方授权。

GNU Radio 3.x 和 `gr-lilacsat` 属 GPL 系依赖。MIT 源码可以与 GPL 组件配合使用，但包含和链接这些组件的完整二进制发行物还必须满足相应 GPL 源码、许可证和版权告知义务。SDR# 与另行提供的 Windows 旧上传代理不属于本项目 MIT 授权范围；该说明不适用于仓库内的 Linux 原生代理源码。

本项目与 SDR#、GNU Radio 及所支持卫星的原作者或运营方不存在官方隶属或背书关系，除非相应权利方另有书面说明。
