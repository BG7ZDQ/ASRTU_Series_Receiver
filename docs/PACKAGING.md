# Windows 打包

## 便携版

`packaging/package_portable.ps1` 从 Release 构建复制程序、Qt 平台插件、GNU Radio/Qt 运行库、翻译 `.qm` 和必要许可证文件。

## 安装器

安装器使用 Inno Setup 6，版本号当前为 1.5.4。全新 Windows 环境只需安装 Visual Studio 2022 Build Tools 和 Inno Setup 6，然后运行：

```powershell
.\build_installer.ps1
```

该默认路径会重新编译 SDR# 桥接插件，从 `packaging/payload/` 创建干净的 staging，并生成 `packaging/inno/dist/ASRTU_Series_Receiver_Setup.exe`。安装器启动时始终由用户从简体中文、English、日本語中选择安装向导语言。

GitHub Actions 的 `windows-installer` job 会下载已经通过启动冒烟测试的 Windows 便携包，在 `windows-2022` runner 上调用 Inno Setup 6 生成同一安装器，并校验文件大小和 SHA-256。正式版本标签的发布任务同时上传安装器 EXE、便携 ZIP、DEB、RPM 和 AppImage；若 runner 镜像未预装 Inno Setup，工作流会通过 Chocolatey 安装后再构建。

若还要重编 DSP、启动器和代理包装器：

```powershell
.\packaging\inno\build_installer.ps1 -RebuildDsp
```

此路径另外要求安装 radioconda/GNU Radio、Qt、Qwt 和 `gr-lilacsat` 开发环境。

## 第三方再分发

`packaging/payload/` 中的第三方运行组件不适用本项目 MIT 许可，仍须保留各自条款、版权声明和安装器内的第三方告知。
