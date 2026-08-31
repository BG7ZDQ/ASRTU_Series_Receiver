# 架构与数据流

## 组件

`ASRTU1_Launcher` 负责选择输入、声卡、录音选项和地面站资料，并启动其余进程。`ASRTU1_Demod_CQt` 执行 DSP、FEC、绘图和日志记录。`ASRTU_Proxy` 为旧 MMT 上传代理提供独立图标和控制台窗口。`ASRTU_SatnogsUploader` 是 Windows/Linux 共用的 Qt 上传程序，订阅 PMT/ZMQ 帧、显示站点与服务器状态并通过 HTTPS 上传 SatNOGS。`SDRSharp.AstroSeriesBridge` 从 SDR# 输出本地 RAW I/Q，并读取自动多普勒控制数据。

## 信号流

```text
SDR# shared memory ─┐
stereo zero-IF ─────┼─> normalization / channel selection ─> BPSK loop
mono +12 kHz IF ────┤                                      ├─> constellation/SNR
WAV/OGG playback ───┘                                      └─> Viterbi + FEC
                                                                      │
                                                        TCP PDU 127.0.0.1:9985
                                                                      │
                                                   ┌────── MMT proxy/WebSocket
                                                   └────── SatNOGS uploader/HTTPS
```

单声道实数中频的输入瀑布图和输入频谱在 `float` 转为复数之前取样，并把 12 kHz 显示为图中的 0 Hz；解调支路再完成频移与复数化。

## 进程间通信

- 解码帧：TCP PDU，默认 `127.0.0.1:9985`
- 上传发布：ZMQ PUB，默认 `127.0.0.1:5555`。线上消息是 GNU Radio PMT
  序列化的 PDU；订阅端必须使用 PMT 反序列化，并验证载荷为 223 字节，不能
  假定或直接跳过固定长度的序列化头。
- SatNOGS 上传程序设置最多 4 个并发请求和 128 帧等待队列，避免网络变慢时
  无限制占用内存。文件回放不会启用网络输出，因此不会上传历史帧。
- SDR# RAW I/Q：Windows 本地共享内存
- 自动多普勒：`Local\\ASRTU_DOPPLER_CONTROL_V1` 共享内存映射

## 运行数据

录音和日志在 Windows 默认写入安装目录旁的 `ASRTU1_Records/<timestamp>/`，在 Linux 写入 `QStandardPaths::GenericDataLocation/ASRTU/ASRTU1_Records/<timestamp>/`。TLE 聚合缓存保存到当前用户的应用数据目录。呼号和坐标只写入本机配置，不应提交到版本库。
