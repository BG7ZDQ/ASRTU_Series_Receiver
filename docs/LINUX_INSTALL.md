# Linux AppImage 安装说明

Linux 发行包是一个 ZIP，第一层是 `setup.sh`，第二层目录里放 AppImage、
SDR++ 和 ASRTU 共享内存插件：

```text
ASRTU_Series_Receiver-<版本号>-x86_64-LinuxAppImage.zip
  setup.sh
  ASRTU_Series_Receiver-<版本号>-x86_64-LinuxAppImage/
    ASRTU_Series_Receiver-<版本号>-x86_64-LinuxAppImage.AppImage
    plugins/asrtu_sink.so
    sdrpp/sdrpp_ubuntu_jammy_amd64.deb
```

## 系统要求

- x86_64 Linux
- 推荐 Ubuntu 22.04 / Jammy 或兼容发行版（Zorin OS 17 可以直接使用）
- 只安装 ASRTU 时不需要 `sudo`
- 安装随包 SDR++ 时需要 `sudo`、网络和 APT；SDR++ 的 Debian 包是
  Ubuntu 22.04 amd64 版本
- 不需要 `libfuse2`：`setup.sh` 会解包 AppImage 后安装

## 安装

在解压目录执行：

```bash
bash setup.sh
```

脚本会依次询问：

1. **安装语言**：简体中文 / English / 日本語
2. **是否安装 SDR++**：选择否时只安装 ASRTU

也可以直接指定：

```bash
# 只安装 ASRTU
bash setup.sh --lang zh --without-sdrpp --yes

# 安装 ASRTU、SDR++ 和共享内存插件
bash setup.sh --lang zh --with-sdrpp --yes
```

`--yes` 自动确认覆盖安装和可选 SDR++ 安装；若不需要 SDR++，请同时传入
`--without-sdrpp`。它不会跳过 `sudo` 密码输入。`--lang` 选择安装提示语言，
应用界面默认跟随系统语言；也可用 `asrtu --language=zh`（或 `en`、`ja`）指定。

自定义目录可以包含空格，安装和卸载时使用同一个 `--prefix`：

```bash
bash setup.sh --lang zh --without-sdrpp --prefix "$HOME/Applications/ASRTU Receiver"
bash setup.sh --lang zh --uninstall --prefix "$HOME/Applications/ASRTU Receiver"
```

安装器会将路径规范化，并拒绝系统目录、个人主目录及安装包所在目录的祖先。
已有目录必须带有本安装器写入的 `.asrtu-installation` 标记，否则不会覆盖或
卸载。旧版安装没有此标记时，请先手动重命名旧目录作为备份，或选择新目录；
安装器不会自动认领或迁移旧目录，也不接受符号链接作为安装目标。

## 安装后的位置

- ASRTU 程序：`~/.local/opt/asrtu`
- 命令：`~/.local/bin/asrtu`
- 应用菜单：`ASRTU Series Receiver`
- 桌面快捷方式：`~/Desktop/asrtu-series-receiver.desktop`
- SDR++：由 Debian 包安装到 `/usr/bin/sdrpp` 和 `/usr/lib/sdrpp`
- ASRTU SDR++ 插件：`/usr/lib/sdrpp/plugins/asrtu_sink.so`

首次登录或执行 `source ~/.profile` 后，终端可以直接运行：

```bash
asrtu
sdrpp
```

## 在 SDR++ 中启用 ASRTU

安装 SDR++ 不会自动改掉你已有的 SDR++ 配置。首次使用时手动配置一次：

1. 打开 SDR++
2. 在 **Module Manager** 中添加 `asrtu_sink` 实例，命名为 **ASRTU Bridge**
   并启用；已有实例则直接启用
3. **Radio** 选择 **RAW**，带宽设为 **48 kHz**
4. **Sinks** 选择 **ASRTU Receiver**
5. 回到 ASRTU 启动器，输入选择
   **本地共享内存 RAW 模式 I/Q 桥接**

SDR++ 的多普勒来自 TinyDoppler 的共享内存控制记录，不需要
**Rigctl Server**，也不需要把 SDR++ 的 Rigctl 端口填到 ASRTU 里。
只有实体电台的 **电台控频…** 才使用 Hamlib `rigctld`（默认 4534）。
已有 Rigctl 配置由用户自行管理，ASRTU 不删除或迁移这些设置。

## 卸载

```bash
bash setup.sh --uninstall
```

卸载会删除 ASRTU 程序、命令、菜单和桌面快捷方式，但保留用户数据与录音。
它不会自动卸载 SDR++，以免误删用户自己安装的软件。

## 常见问题

### AppImage 无法直接双击

不需要对 AppImage 设置执行权限后直接运行；请使用 `setup.sh`。脚本会解包
AppImage 到用户目录，因此不依赖 FUSE。

### `asrtu` 命令找不到

桌面和应用菜单入口使用绝对路径，不受 PATH 影响。终端使用前重新登录，
或执行：

```bash
source ~/.profile
```

### SDR++ 安装失败

通常是缺少依赖或没有网络。脚本会保留 ASRTU 安装，并提示 SDR++ 失败。
可手动安装：

```bash
sudo apt-get update
sudo apt-get install -y libfftw3-dev libglfw3-dev libvolk2-dev \
  librtaudio-dev libzstd-dev
sudo apt-get install -y ./sdrpp/sdrpp_ubuntu_jammy_amd64.deb
sudo install -Dm755 plugins/asrtu_sink.so \
  /usr/lib/sdrpp/plugins/asrtu_sink.so
```

### 插件加载失败

SDR++ 与插件必须使用匹配的 ABI。随包插件针对 SDR++ 1.3.2 构建；如果你
系统里已有另一版本 SDR++，建议先卸载旧版本再使用随包版本，或按
[docs/BUILDING.md](BUILDING.md) 重新编译插件。
