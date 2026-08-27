from pathlib import Path
import xml.etree.ElementTree as ET


ROOT = Path(__file__).resolve().parents[1]
TS = ROOT / "assets" / "translations" / "asrtu_en.ts"

TRANSLATIONS = {
    "I/Q 输入异常": "I/Q Input Mismatch",
    "检测到 I/Q 两路幅度严重不平衡，当前输入可能是单声道 USB/实数音频，因此频谱会出现镜像。\n请在启动器中改选“单声道实数域 12KHz 电台 IF 输入”。": "The I and Q channel levels are severely imbalanced. The input may be mono USB/real audio, which produces a mirrored spectrum.\nSelect ‘Mono Real 12 kHz Radio IF Input’ in the launcher.",
    "无法创建录音和日志目录：\n%1": "Unable to create the recording and log directory:\n%1",
    "找不到程序：\n%1": "Program not found:\n%1",
    "无法启动：\n%1": "Unable to start:\n%1",
    "尚未配置上传信息，请先保存设置。": "Upload information is not configured. Save the settings first.",
    "上传代理启动后立即退出（代码 %1）。": "The upload proxy exited immediately after launch (code %1).",
    "\n日志：%1": "\nLog: %1",
    "\n\n最后输出：\n%1": "\n\nLast output:\n%1",
    "找不到录音格式转换器。 ": "Audio format converter not found. ",
    "无法转换录音文件：/%1": "Unable to convert the recording: /%1",
    "仅支持单声道或双声道录音文件。": "Only mono or stereo recordings are supported.",
    "所选卫星没有可用的 SatNOGS NORAD ID。": "The selected satellite has no available SatNOGS NORAD ID.",
    "阿斯图系列卫星启动器": "ASRTU Series Satellite Launcher",
    "阿斯图系列卫星接收与遥测上传": "ASRTU Series Satellite Reception and Telemetry Upload",
    "配置接收输入与地面站资料，再按需启动各个组件。": "Configure the receiver input and ground-station details, then start each component as needed.",
    "接收与上传设置": "Reception and Upload Settings",
    "本地内存共享 RAW 模式 I/Q 桥接": "Local Shared-Memory RAW I/Q Bridge",
    "立体声零中频 RAW 模式 I/Q 输入": "Stereo Zero-IF RAW I/Q Input",
    "单声道实数域 12KHz 电台 IF 输入": "Mono Real 12 kHz Radio IF Input",
    "输入声道不匹配": "Input Channel Mismatch",
    "打开录音目录": "Open Recordings Folder",
    "快速重放录音": "Quick Replay Recording",
    "快速重放失败": "Quick Replay Failed",
    "多普勒工具启动失败": "Unable to Start Doppler Tool",
    "正在下载第 %1/%2 个 TLE 来源": "Downloading TLE source %1 of %2",
    "TLE 更新完成：%1/%2 个来源": "TLE update complete: %1 of %2 sources",
    "在线更新失败，继续使用本地 TLE 数据": "Online update failed; continuing with local TLE data",
    "尚未配置上传信息，请先填写呼号和地面站资料。": "Upload information is not configured. Enter the call sign and ground-station details first.",
    "当前模式需要双声道 I/Q，但所选声卡仅提供单声道。\n若输入来自电台 12 kHz 中频或 SDR# 的 USB 音频，请改选“单声道实数域 12KHz 电台 IF 输入”。": "This mode requires stereo I/Q, but the selected audio device provides only one channel.\nFor a radio 12 kHz IF or SDR# USB audio, select ‘Mono Real 12 kHz Radio IF Input’.",
    "自动保存本次接收录音": "Automatically save this reception recording",
    "同时上传至 SatNOGS": "Also upload to SatNOGS",
    "输入": "Input",
    "声卡": "Sound card",
    "呼号": "Callsign",
    "经度": "Longitude",
    "纬度": "Latitude",
    "海拔": "Altitude",
    "常用工具": "Tools",
    "打开 SDR# 遥测预设": "Open SDR# Telemetry Preset",
    "卫星跟踪与自动多普勒": "Satellite / Doppler Tracking",
    "播放录音文件": "Play Recording File",
    "快速播放录音文件": "Quick Play Recording File",
    "打开录音与日志": "Open Recordings and Logs",
    "就绪": "Ready",
    "保存设置": "Save Settings",
    "启动接收": "Start Receiver",
    "启动上传代理": "Start Upload Proxy",
    "启动上传": "Start Upload",
    "上传设置": "Upload Settings",
    "SDR# 启动失败": "Failed to Start SDR#",
    "无法打开目录": "Unable to Open Directory",
    "选择接收录音": "Select Reception Recording",
    "接收录音 (*.wav *.ogg *.oga *.opus *.flac *.mp3)": "Reception Recordings (*.wav *.ogg *.oga *.opus *.flac *.mp3)",
    "播放失败": "Playback Failed",
    "正在播放：/%1；日志：%2": "Playing: /%1; log: %2",
    "快速播放失败": "Quick Playback Failed",
    "正在重放：/%1/%2": "Replaying: /%1/%2",
    "上传代理启动失败": "Failed to Start Upload Proxy",
    "上传代理已启动（PID %1）": "Upload proxy started (PID %1)",
    "启动失败": "Startup Failed",
    "接收已启动；录音与日志：%1": "Receiver started; recording and logs: %1",
    "接收已启动；SatNOGS 上传已启用；录音与日志：%1": "Receiver started; SatNOGS upload enabled; recording and logs: %1",
    "系统默认输入设备": "System Default Input Device",
    "选择上传卫星": "Select Upload Satellite",
    "本次遥测上传目标：": "Telemetry upload target for this session:",
    "资料不完整": "Incomplete Information",
    "请填写呼号或昵称。": "Enter a callsign.",
    "保存失败": "Save Failed",
    "无法写入：\n%1": "Unable to write:\n%1",
    "配置文件提交失败。": "Failed to commit the configuration file.",
    "设置已保存": "Settings saved",
    "阿斯图系列卫星启动失败": "Failed to Start the ASRTU Series Satellite Suite",
    "阿斯图系列卫星跟踪与多普勒": "ASRTU Series Satellite Tracking and Doppler",
    "本地缓存": "local cache",
    "已载入本地 TLE 缓存，正在在线更新…": "Local TLE cache loaded; updating online…",
    "实时跟踪数据": "Live Tracking Data",
    "等待 TLE 数据": "Waiting for TLE data",
    "方位角": "Azimuth",
    "仰角": "Elevation",
    "距离": "Range",
    "多普勒": "Doppler",
    "接收频率": "Receive Frequency",
    "TLE 历元": "TLE Epoch",
    "卫星与频率": "Satellite and Frequency",
    "卫星": "Satellite",
    "频率预设": "Frequency Preset",
    "标称下行": "Nominal Downlink",
    "TLE 来源（每行一个，全部下载并合并）": "TLE Sources (one per line; all are downloaded and merged)",
    "等待更新": "Waiting for update",
    "立即更新 TLE": "Update TLE Now",
    "打开星历目录": "Open Ephemeris Directory",
    "地面站：%1°, %2°，%3 m": "Ground station: %1°, %2°, %3 m",
    "正在下载：%1": "Downloading: %1",
    "在线合并": "online merge",
    "所有 TLE 来源均下载失败": "All TLE sources failed to download",
    "在线更新失败，继续使用本地合并文件": "Online update failed; continuing with the local merged file",
    "已下载 %1/%2 个来源，合并 %3 颗卫星%4：all_sources.tle": "Downloaded %1/%2 sources and merged %3 satellites%4: all_sources.tle",
    "并保存": " and saved",
    "，但保存失败": ", but saving failed",
    "自定义": "Custom",
    "SGP4 计算失败": "SGP4 calculation failed",
    "阿斯图系列卫星接收解码": "ASRTU Series Satellite Receiver and Decoder",
}


tree = ET.parse(TS)
missing = []
for message in tree.findall(".//message"):
    source = message.findtext("source", default="")
    translation = message.find("translation")
    if source not in TRANSLATIONS:
        missing.append(source)
        continue
    translation.attrib.pop("type", None)
    translation.text = TRANSLATIONS[source]

if missing:
    raise SystemExit("Missing translations: " + repr(missing))

if hasattr(ET, "indent"):
    ET.indent(tree, space="    ")
tree.write(TS, encoding="utf-8", xml_declaration=True)
