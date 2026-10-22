# Windows 打包

## 便携版

`packaging/package_portable.ps1` 从 Release 构建复制程序、Qt 平台插件、GNU Radio/Qt 运行库、翻译 `.qm` 和必要许可证文件。

## 安装器

安装器使用 Inno Setup 6，版本号当前为 1.5.4。全新 Windows 环境只需安装 Visual Studio 2022 Build Tools 和 Inno Setup 6，然后运行：

```powershell
.\build_installer.ps1
```

该默认路径会重新编译 SDR# 桥接插件，从 `packaging/payload/` 创建干净的 staging，并生成 `packaging/inno/dist/ASRTU_Series_Receiver-<版本号>-WinInstaller(Recommend).exe`（例如 `ASRTU_Series_Receiver-1.5.4-WinInstaller(Recommend).exe`，版本号取自 `CMakeLists.txt`）。安装器启动时始终由用户从简体中文、English、日本語中选择安装向导语言。若本地没有 `packaging/payload/sdrpp` 或 `packaging/payload/plutosdr-driver`，脚本会跳过对应可选组件并输出警告；此时生成的是核心安装器（仍包含 SDR# 预设），完整安装器需要本地补齐这两个载荷。

GitHub Actions 的 `windows-installer` job 会下载已经通过启动冒烟测试的 Windows 便携包，在 `windows-2022` runner 上调用 Inno Setup 6 生成同一安装器，并校验文件大小和 SHA-256。Windows 便携包命名为 `ASRTU_Series_Receiver-<版本号>-WinPortable.zip`；正式版本标签还会上传 `LinuxAppImage` 后缀的 Linux AppImage。若 runner 镜像未预装 Inno Setup，工作流会通过 Chocolatey 重新安装后再构建。简体中文、English 和日本語安装向导消息随仓库固定版本，不依赖 runner 本地 Inno Setup 的语言文件。

若还要重编 DSP、启动器和代理包装器：

```powershell
.\packaging\inno\build_installer.ps1 -RebuildDsp
```

此路径另外要求安装 radioconda/GNU Radio、Qt、Qwt 和 `gr-lilacsat` 开发环境。

## SDR++ 插件

`integrations/sdrpp/` 提供 Windows `asrtu_sink` 共享内存插件。SDR++ 的
RAW 模式输出以 48 kHz 浮点 I/Q 写入与 SDR# 相同的共享内存，多普勒指令
也从 TinyDoppler 共享内存读取。启动器使用 `--shared-iq-bridge`，不依赖
UDP 或 Rigctl。新版 SDR# 插件与 SDR++ 插件使用同一所有权锁，后启动的
桥接实例会被拒绝，不能发送 I/Q 或调频。详细配置和构建步骤见
[SDR++ 插件说明](../integrations/sdrpp/README.md)。

插件须针对目标 SDR++ 的头文件、核心导入库与架构编译。附带 SDR++ 的本地
便携包应包含 `sdrpp/modules/asrtu_sink.dll`，并将 Radio sink 设置为
`ASRTU Receiver`；同时更新 TinyDoppler 和 SDR# 插件。仅提供源码的 CI
包仍需用户针对自己的 SDR++ 编译插件。Linux 暂不提供共享内存输入。

Windows 安装包同时包含 PlutoSDR-M2K 官方 USB 驱动文件及其管理员安装入口，
以及 libiio 0.26 的 USB/IP 后端。SDR++ 的 Pluto 模块支持手动输入完整
libiio URI（例如 `ip:192.168.1.10`）。SDR++ 与 SDR# 共用 rtl-sdr 2.0.2
驱动，以支持 RTL-SDR Blog V4 / R828D。

## 第三方再分发

`packaging/payload/` 中的第三方运行组件不适用本项目 MIT 许可，仍须保留各自条款、版权声明和安装器内的第三方告知。
