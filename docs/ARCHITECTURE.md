# 架构与数据流

## 组件

`ASRTU1_Launcher` 负责选择输入、声卡、录音选项和地面站资料，并启动其余进程。`ASRTU1_Demod_CQt` 执行 DSP、FEC、绘图和日志记录。`ASRTU_Proxy` 为旧 MMT 上传代理提供独立图标和控制台窗口。`ASRTU_SatnogsUploader` 是 Windows/Linux 共用的 Qt 上传程序，订阅 PMT/ZMQ 帧、显示站点与服务器状态并通过 HTTPS 上传 SatNOGS。独立的 `external/TinyDoppler/` 子模块提供卫星跟踪应用及 SDR# 插件；插件从 SDR# 输出本地 RAW I/Q，并读取自动多普勒控制数据。

## 信号流

```text
SDR# shared memory ─┐
stereo zero-IF ─────┼─> normalization / channel selection ─> BPSK loop
mono +12 kHz IF ────┤                                      ├─> constellation
WAV/OGG playback ───┘                                      └─> Viterbi + FEC
                                                                      │
                                                        TCP PDU 127.0.0.1:9985
                                                                      │
                                                   ┌────── MMT proxy/WebSocket
                                                   └────── SatNOGS uploader/HTTPS
```

输入频谱和输入瀑布图统一从主解调链的 AGC 输出取样。单声道实数中频先完成
12 kHz 中心频率搬移、低通滤波、固定增益和 AGC，再同时送往频谱/瀑布图与
FLL/时钟/Costas 解调链。

环路锁定时，SNR 取自并行 OpenHoshimi 解码器的 Gardner/Costas 符号输出，
计算符号端 Es/N0；未锁定时回退到主链 SVR 盲估计，并标明算法。
两种估计均不依赖有效 RS 帧。首次有效估计前显示 `--`，无效估计或断流时
保留最后的有效读数及历史，断流后锁定指示仍会熄灭。
图表始终保留零刻度并使用整数网格，
详见 [DSP 说明](DSP.md)。

## 进程间通信

- 解码帧：TCP PDU，默认 `127.0.0.1:9985`
- 上传发布：ZMQ PUB，默认 `127.0.0.1:5555`。线上消息是 GNU Radio PMT
  序列化的 PDU；订阅端必须使用 PMT 反序列化，并验证载荷为 223 字节，不能
  假定或直接跳过固定长度的序列化头。
- SatNOGS 上传程序设置最多 4 个并发请求和 128 帧等待队列，避免网络变慢时
  无限制占用内存。文件回放不会启用网络输出，因此不会上传历史帧。
- SDR# / SDR++ RAW I/Q：Windows 共用本地共享内存
- 自动多普勒：`Local\\ASRTU_DOPPLER_CONTROL_V1` 共享内存映射

## 运行数据

录音和日志在 Windows 默认写入安装目录旁的 `ASRTU1_Records/<timestamp>/`，在 Linux 写入 `QStandardPaths::GenericDataLocation/ASRTU/ASRTU1_Records/<timestamp>/`。Tiny Doppler 的卫星目录和轨道数据缓存分别保存在当前用户的配置/应用数据目录。呼号和坐标只写入本机配置，不应提交到版本库。
