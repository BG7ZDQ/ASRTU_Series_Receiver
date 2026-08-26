# Windows 打包

## 便携版

`asrtu-qt/package_portable.ps1` 从 Release 构建复制程序、Qt 平台插件、GNU Radio/Qt 运行库、英文 `.qm` 和必要许可证文件。

## 安装器

安装器使用 Inno Setup 6，版本号当前为 1.5.0。完整套件构建脚本会编译解码器和 SDR# 插件、整理 staging 目录并生成安装器：

```powershell
.\asrtu-suite\build_installer.ps1 `
  -ProxySource C:\path\to\proxy_mmt_gui `
  -SdrSharpPresetSource C:\path\to\SDRSharp `
  -SdrSharpApiRoot C:\path\to\SDRSharp
```

生成文件为 `asrtu-suite/dist/ASRTU_Series_Receiver_Setup.exe`。安装器启动时始终由用户从简体中文、English、日本語中选择安装向导语言，不根据系统语言自动决定，也不沿用上一次安装语言。应用运行时语言仍由系统 locale 决定，并可用调试参数强制指定。

## 第三方再分发

源码发布不包含上传代理、SDR# 或其他第三方二进制。制作公开安装包前必须确认其许可允许捆绑再分发，并随包附带相应版权与许可证。不能确认时，应只发布本项目源码和让用户自行提供依赖的构建说明。
