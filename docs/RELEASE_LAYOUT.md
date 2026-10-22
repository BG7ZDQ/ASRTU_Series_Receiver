# Release artifact layout

All release artifacts use the project version from `CMakeLists.txt` and a
platform/package suffix:

| Platform | Package | Filename pattern |
| --- | --- | --- |
| Windows x64 | Portable archive | `ASRTU_Series_Receiver-<version>-WinPortable.zip` |
| Windows x64 | Recommended installer | `ASRTU_Series_Receiver-<version>-WinInstaller(Recommend).exe` |
| Linux x64 | AppImage | `ASRTU_Series_Receiver-<version>-LinuxAppImage.AppImage` |

The Linux AppImage job builds on Ubuntu 22.04. The Windows portable archive is
smoke-tested before the installer is created from the same payload. Debian and
RPM packages are no longer built.
macOS is currently documented as a porting branch only and is not merged into
the release workflow.
