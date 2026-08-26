# 架构与数据流

## 组件

`ASRTU1_Launcher` 负责选择输入、声卡、录音选项和地面站资料，并启动其余进程。`ASRTU1_Demod_CQt` 执行 DSP、FEC、绘图和日志记录。`ASRTU_Proxy` 为上传代理提供独立图标和控制台窗口。`SDRSharp.AstroSeriesBridge` 从 SDR# 输出本地 RAW I/Q，并读取自动多普勒控制数据。

## 信号流

```text
SDR# shared memory ─┐
stereo zero-IF ─────┼─> normalization / channel selection ─> BPSK loop
mono +12 kHz IF ────┤                                      ├─> constellation/SNR
WAV/OGG playback ───┘                                      └─> Viterbi + FEC
                                                                      │
                                                        TCP PDU 127.0.0.1:9985
                                                                      │
                                                          telemetry proxy/upload
```

单声道实数中频的输入瀑布图和输入频谱在 `float` 转为复数之前取样，并把 12 kHz 显示为图中的 0 Hz；解调支路再完成频移与复数化。

## 进程间通信

- 解码帧：TCP PDU，默认 `127.0.0.1:9985`
- 上传发布：ZMQ PUB，默认 `127.0.0.1:5555`
- SDR# RAW I/Q：Windows 本地共享内存
- 自动多普勒：`Local\\ASRTU_DOPPLER_CONTROL_V1` 共享内存映射

## 运行数据

录音和日志默认写入安装目录旁的 `ASRTU1_Records/<timestamp>/`。TLE 聚合缓存保存到当前用户的应用数据目录。呼号和坐标只写入本机配置，不应提交到版本库。
