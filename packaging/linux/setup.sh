#!/usr/bin/env bash
# ASRTU Linux AppImage installer. Installs into the current user's home
# directory; SDR++ is optional and is installed from the bundled Debian
# package only when the user selects it.
set -euo pipefail

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
LANG_CODE=""
WITH_SDRPP=""
PREFIX="${HOME}/.local/opt/asrtu"
ASSUME_YES=0
UNINSTALL=0

usage() {
    echo "usage: $0 [--lang zh|en|ja] [--with-sdrpp|--without-sdrpp] [--prefix DIR] [--yes] [--uninstall]"
}

while (($#)); do
    case "$1" in
        --lang) LANG_CODE="${2:?missing language}"; shift 2 ;;
        --with-sdrpp) WITH_SDRPP=yes; shift ;;
        --without-sdrpp) WITH_SDRPP=no; shift ;;
        --prefix) PREFIX="${2:?missing prefix}"; shift 2 ;;
        --yes) ASSUME_YES=1; shift ;;
        --uninstall) UNINSTALL=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) echo "unknown argument: $1" >&2; usage >&2; exit 2 ;;
    esac
done

select_language() {
    if [[ -n "$LANG_CODE" ]]; then
        case "$LANG_CODE" in
            zh|en|ja) return ;;
            *) echo "unsupported language: $LANG_CODE" >&2; exit 2 ;;
        esac
    fi
    echo
    echo "1) 简体中文"
    echo "2) English"
    echo "3) 日本語"
    printf "安装语言 / Language / 言語 [1-3, default 1]: "
    local choice=""
    read -r choice || true
    case "$choice" in
        2) LANG_CODE=en ;;
        3) LANG_CODE=ja ;;
        *) LANG_CODE=zh ;;
    esac
}

say() {
    local key="$1"
    case "$LANG_CODE:$key" in
        zh:welcome) echo "ASRTU Linux 安装程序" ;;
        en:welcome) echo "ASRTU Linux installer" ;;
        ja:welcome) echo "ASRTU Linux インストーラー" ;;
        zh:app_found) echo "已找到 AppImage：%s" ;;
        en:app_found) echo "Found AppImage: %s" ;;
        ja:app_found) echo "AppImage が見つかりました: %s" ;;
        zh:no_app) echo "安装包中缺少 AppImage 文件。" ;;
        en:no_app) echo "The AppImage file is missing from this package." ;;
        ja:no_app) echo "パッケージに AppImage がありません。" ;;
        zh:existing) echo "目标目录已存在：%s" ;;
        en:existing) echo "Target directory already exists: %s" ;;
        ja:existing) echo "インストール先が既に存在します: %s" ;;
        zh:overwrite) echo "是否覆盖现有安装？[y/N] " ;;
        en:overwrite) echo "Overwrite the existing installation? [y/N] " ;;
        ja:overwrite) echo "既存のインストールを上書きしますか？[y/N] " ;;
        zh:cancelled) echo "已取消。" ;;
        en:cancelled) echo "Cancelled." ;;
        ja:cancelled) echo "キャンセルしました。" ;;
        zh:installing) echo "正在安装 ASRTU 到 %s ..." ;;
        en:installing) echo "Installing ASRTU into %s ..." ;;
        ja:installing) echo "ASRTU を %s にインストールしています..." ;;
        zh:extract_failed) echo "AppImage 解包失败。请确认文件完整并且系统有执行权限。" ;;
        en:extract_failed) echo "AppImage extraction failed. Check the file and its execute permission." ;;
        ja:extract_failed) echo "AppImage の展開に失敗しました。ファイルと実行権限を確認してください。" ;;
        zh:sdrpp_prompt) echo "是否同时安装可选的 SDR++ 及 ASRTU 共享内存插件？[y/N] " ;;
        en:sdrpp_prompt) echo "Also install optional SDR++ and the ASRTU shared-memory plugin? [y/N] " ;;
        ja:sdrpp_prompt) echo "オプションの SDR++ と ASRTU 共有メモリプラグインもインストールしますか？[y/N] " ;;
        zh:sdrpp_missing) echo "安装包未包含 SDR++，将只安装 ASRTU。" ;;
        en:sdrpp_missing) echo "This package does not contain SDR++, so only ASRTU will be installed." ;;
        ja:sdrpp_missing) echo "このパッケージには SDR++ が含まれていないため、ASRTU のみをインストールします。" ;;
        zh:sdrpp_installing) echo "正在安装 SDR++ 和共享内存插件 ..." ;;
        en:sdrpp_installing) echo "Installing SDR++ and the shared-memory plugin ..." ;;
        ja:sdrpp_installing) echo "SDR++ と共有メモリプラグインをインストールしています..." ;;
        zh:sdrpp_failed) echo "SDR++ 安装失败；ASRTU 已安装，可稍后手动安装 SDR++。" ;;
        en:sdrpp_failed) echo "SDR++ installation failed; ASRTU is installed and SDR++ can be installed later." ;;
        ja:sdrpp_failed) echo "SDR++ のインストールに失敗しました。ASRTU はインストール済みです。" ;;
        zh:sdrpp_done) echo "SDR++ 与共享内存插件已安装。" ;;
        en:sdrpp_done) echo "SDR++ and the shared-memory plugin are installed." ;;
        ja:sdrpp_done) echo "SDR++ と共有メモリプラグインをインストールしました。" ;;
        zh:sdrpp_no_deb) echo "没有找到 SDR++ 安装包，跳过 SDR++。" ;;
        en:sdrpp_no_deb) echo "SDR++ package not found; skipping SDR++." ;;
        ja:sdrpp_no_deb) echo "SDR++ パッケージが見つからないためスキップします。" ;;
        zh:done) echo "安装完成。运行 asrtu，或从应用菜单打开 ASRTU Series Receiver。" ;;
        en:done) echo "Installation complete. Run asrtu or open ASRTU Series Receiver from the application menu." ;;
        ja:done) echo "インストール完了。asrtu を実行するか、アプリメニューから ASRTU Series Receiver を開いてください。" ;;
        zh:uninstall) echo "正在卸载 ASRTU ..." ;;
        en:uninstall) echo "Uninstalling ASRTU ..." ;;
        ja:uninstall) echo "ASRTU をアンインストールしています..." ;;
        zh:uninstalled) echo "ASRTU 已卸载。用户数据和录音仍保留。" ;;
        en:uninstalled) echo "ASRTU was removed. User data and recordings were kept." ;;
        ja:uninstalled) echo "ASRTU を削除しました。ユーザーデータと録音は保持されています。" ;;
        *) echo "$key" ;;
    esac
}

confirm() {
    local prompt_key="$1"
    if ((ASSUME_YES)); then
        return 0
    fi
    printf "$(say "$prompt_key")"
    local answer=""
    read -r answer || true
    [[ "$answer" =~ ^[Yy]$ ]]
}

select_language
printf '\n== %s ==\n\n' "$(say welcome)"

if ((UNINSTALL)); then
    printf "$(say uninstall)\n"
    rm -rf -- "$PREFIX"
    rm -f -- "${HOME}/.local/bin/asrtu"
    rm -f -- "${HOME}/.local/share/applications/asrtu-series-receiver.desktop"
    rm -f -- "${HOME}/Desktop/asrtu-series-receiver.desktop"
    printf "%s\n" "$(say uninstalled)"
    exit 0
fi

BUNDLE_DIR=$(find "$SCRIPT_DIR" -mindepth 1 -maxdepth 1 -type d -name 'ASRTU_Series_Receiver-*-LinuxAppImage' -print -quit)
if [[ -z "$BUNDLE_DIR" ]]; then
    echo "$(say no_app)" >&2
    exit 1
fi
APPIMAGE=$(find "$BUNDLE_DIR" -maxdepth 1 -type f -name '*.AppImage' -print -quit)
if [[ -z "$APPIMAGE" ]]; then
    echo "$(say no_app)" >&2
    exit 1
fi
printf "$(say app_found)\n" "$APPIMAGE"

if [[ -e "$PREFIX" ]] && ! confirm overwrite; then
    echo "$(say cancelled)"
    exit 1
fi
printf "$(say installing)\n" "$PREFIX"

WORK=$(mktemp -d "${TMPDIR:-/tmp}/asrtu-install.XXXXXX")
trap 'rm -rf -- "$WORK"' EXIT
chmod +x "$APPIMAGE"
if ! (cd "$WORK" && "$APPIMAGE" --appimage-extract >/dev/null 2>&1); then
    echo "$(say extract_failed)" >&2
    exit 1
fi
if [[ ! -d "$WORK/squashfs-root" ]]; then
    echo "$(say extract_failed)" >&2
    exit 1
fi
mkdir -p "$(dirname -- "$PREFIX")"
rm -rf -- "$PREFIX"
mv "$WORK/squashfs-root" "$PREFIX"
chmod 755 "$PREFIX" "$PREFIX/AppRun"

mkdir -p "${HOME}/.local/bin" "${HOME}/.local/share/applications"
cat >"${HOME}/.local/bin/asrtu" <<EOF
#!/bin/sh
exec "$PREFIX/AppRun" "\$@"
EOF
chmod 755 "${HOME}/.local/bin/asrtu"
for profile in "${HOME}/.profile" "${HOME}/.bashrc"; do
    [[ -e "$profile" ]] || : >>"$profile"
    if ! grep -Fq 'ASRTU local bin' "$profile"; then
        {
            echo ''
            echo '# ASRTU local bin'
            echo 'case ":$PATH:" in'
            echo '  *":$HOME/.local/bin:"*) ;;'
            echo '  *) export PATH="$HOME/.local/bin:$PATH" ;;'
            echo 'esac'
        } >>"$profile"
    fi
done

cat >"${HOME}/.local/share/applications/asrtu-series-receiver.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=ASRTU Series Receiver
Comment=ASRTU satellite receiver and decoder
Exec=$PREFIX/AppRun
Icon=$PREFIX/asrtu-series-receiver.png
Terminal=false
Categories=Network;HamRadio;
EOF
if [[ -d "${HOME}/Desktop" ]]; then
    cp "${HOME}/.local/share/applications/asrtu-series-receiver.desktop" \
       "${HOME}/Desktop/asrtu-series-receiver.desktop"
    chmod 755 "${HOME}/Desktop/asrtu-series-receiver.desktop"
    gio set "${HOME}/Desktop/asrtu-series-receiver.desktop" metadata::trusted true 2>/dev/null || true
fi

SDRPP_DEB=$(find "$BUNDLE_DIR/sdrpp" -maxdepth 1 -type f -name '*.deb' -print -quit 2>/dev/null || true)
SDRPP_PLUGIN=$(find "$BUNDLE_DIR/plugins" -maxdepth 1 -type f -name 'asrtu_sink.so' -print -quit 2>/dev/null || true)

if [[ -z "$WITH_SDRPP" ]]; then
    if [[ -n "$SDRPP_DEB" ]]; then
        if confirm sdrpp_prompt; then WITH_SDRPP=yes; else WITH_SDRPP=no; fi
    else
        WITH_SDRPP=no
        printf "%s\n" "$(say sdrpp_missing)"
    fi
fi

if [[ "$WITH_SDRPP" == yes ]]; then
    if [[ -z "$SDRPP_DEB" || -z "$SDRPP_PLUGIN" ]]; then
        printf "%s\n" "$(say sdrpp_no_deb)"
    elif ! command -v dpkg >/dev/null 2>&1; then
        printf "%s\n" "$(say sdrpp_failed)"
    else
        printf "%s\n" "$(say sdrpp_installing)"
        if dpkg -s sdrpp >/dev/null 2>&1; then
            SDRPP_OK=1
        elif sudo apt-get install -y "$SDRPP_DEB"; then
            SDRPP_OK=1
        elif sudo dpkg -i "$SDRPP_DEB"; then
            SDRPP_OK=1
        else
            SDRPP_OK=0
        fi
        if [[ "${SDRPP_OK:-0}" == 1 ]]; then
            MODULE_DIR=$(dpkg -L sdrpp 2>/dev/null | awk '/\/(modules|plugins)$/ {print; exit}')
            if [[ -z "$MODULE_DIR" ]]; then
                for candidate in /usr/lib/sdrpp/plugins /usr/lib/sdrpp/modules; do
                    if [[ -d "$candidate" ]]; then
                        MODULE_DIR=$candidate
                        break
                    fi
                done
            fi
            if [[ -z "$MODULE_DIR" ]]; then
                MODULE_DIR=/usr/lib/sdrpp/plugins
            fi
            if sudo install -Dm755 "$SDRPP_PLUGIN" "$MODULE_DIR/asrtu_sink.so"; then
                printf "%s\n" "$(say sdrpp_done)"
            else
                printf "%s\n" "$(say sdrpp_failed)"
            fi
        else
            printf "%s\n" "$(say sdrpp_failed)"
        fi
    fi
fi

printf '\n%s\n' "$(say done)"
