# 实验 025：ES8311 麦克风采集与 USB WAV 导出

## 目标

把板载麦克风从“已知 GPIO6 方向”推进到可使用的 PGOS 能力：设备持续显示输入电平、保留最近录音、支持可旁路频域降噪和 VAD 驱动的人声增强、低增益耳返/缓存回放，并可通过 USB 导出标准 WAV。第一轮只做有界本地缓冲，不引入网络音频流或文件系统录音。

## 硬件与实现

- ES8311 控制地址 `0x18`，与 RTC 共用 GPIO15/16 I²C。
- I²S MCLK/BCLK/WS 为 GPIO4/5/7，ADC DATA 为 GPIO6，DAC DATA 为 GPIO8。
- 编解码器模拟 MIC/ADC 使用 `SYSTEM 0x14 = 0x1A`（模拟输入、30 dB PGA）、`ADC EQ 0x1C = 0x6A`。默认 `normal` 档写入 `ADC 0x16 = 0x26`（ADC_SYNC、36 dB scale）和 `ADC 0x17 = 0xD7`（+12 dB）；另提供 `low`（`0x24/0xC8`）与 `high`（`0x27/0xD7`）档。
- I²S 为 8 kHz、16-bit、双声道总线；采集端从有效声道归一为 mono PCM16。
- 音频任务只在麦克风页前台、显式耳返或 USB 抓取期间读取 RX、计算 RMS/Peak/Level，并把最近 6 秒、约 96 KB PCM 放在 PSRAM 环形缓冲；离开页面会关闭耳返并停止 RX/DSP/环形缓冲更新。I²S 播放链和 codec 仍保持初始化，ADC 寄存器级断电需另做真机单变量实验。
- 降噪在写入环形缓冲前执行：约 100 Hz Q15 高通去直流/低频，再由 SpeexDSP 以 8 kHz / 20 ms 固定点频域预处理抑制 `-22 dB` 背景。开关保存在 `pgos_audio/mic_denoise`，沿用 750 ms 合并写；关闭时完全旁路。Speex 工作区在服务启动阶段一次性分配到 PSRAM，采集空闲时保留状态但不执行 FFT。
- 人声增强位于频域降噪之后：BSD-3-Clause [`libfvad`](https://github.com/dpirch/libfvad)（WebRTC VAD）以 8 kHz / 20 ms 帧、mode 3 判断语音；只有连续 3 帧（60 ms）命中且降噪后帧 RMS 不低于 650 才打开，并增加 6 帧（120 ms）退出 hangover。随后 AGC 把语音目标 RMS 拉到 3000，最大 4 倍（约 +12 dB），峰值限制为 30000。开关保存在 `pgos_audio/mic_voice` 并默认开启；`mic voice on` 会自动开启降噪，VAD 未就绪时不放大。VAD 对象也在启动期分配，页面切换和本机回放只重置状态。它仍不是声源分离，不能从单麦克风混合信号中无损剥离音乐或另一段人声。
- 缓存回放和耳返复用现有 DAC；耳返默认关闭并衰减到 35%，避免启动即形成反馈。
- USB 最长导出最近 5 秒；导出前复制到最多约 80 KB 的临时缓冲，避免慢速 CDC 发送期间被实时写指针覆盖。

麦克风页位于 Settings / 麦克风，提供采集状态、实时电平、输入增益、降噪、人声增强、耳返和最近 3 秒回放。各项使用独立的 Lucide 位图图标；USB WAV 导出仅由上位机命令和 `capture_microphone.py` 触发，不作为设备端操作项。页面只访问 `AudioService` 快照和请求接口，不直接操作 I²S 或 ES8311。

输入增益项遵循方向键语义：右键按 `low → normal → high` 正向循环，左键反向循环，确认键等同于右键；其它二值开关仍允许左右键或确认键切换。

`libfvad` 固定导入自 [`dpirch/libfvad`](https://github.com/dpirch/libfvad) commit `532ab666c20d3cfda38bca63abbb0f152706c369`；SpeexDSP 固定导入自 [`xiph/speexdsp`](https://github.com/xiph/speexdsp) commit `7a158783df74efe7c2d1c6ee8363c1e695c71226`。两者的上游许可证和来源说明随所用源码子集保存在 `src/third_party/`；当前不引入 ESP-SR 或神经网络语音分离模型。

## USB 协议与工具

控制命令：

```text
page mic
mic status
mic capture on|off
mic gain low|normal|high
mic denoise on|off|toggle
mic voice on|off|toggle
mic record [250-5000 ms]
mic monitor on|off|toggle
mic playback [ms]
```

`capture_microphone.py` 先以 `mic capture on` 开启 USB 临时采集，等待目标时长后导出，最后用 `mic capture off` 释放请求；固件仍会在直接调用 `mic record` 且当前空闲时做一次有界临时采集。工具使用内部可选 correlation ID，并接收以下原始帧：

```text
PGM1 header (28 bytes: version/format/rate/requested_ms/size/request_id/sequence)
<mono PCM16LE payload>
PGM2 trailer (request_id/sequence/CRC32)
```

主机按 magic 重同步，校验格式、长度、request、sequence 和 CRC32 后才写 WAV。麦克风二进制导出只允许 USB 命令触发，不放入 TCP 控制白名单。

## 2026-07-25 真机验证

```powershell
pio run -e playground
python -m unittest discover -s tools/tests -p "test_*.py"
pio run -e playground -t upload --upload-port COM3
python tools/capture_microphone.py --port COM3 --duration 3000 --output captures/mic-test.wav
```

结果：

- 固件构建通过；当前 RAM 118648 / 327680 bytes（36.2%），Flash 2435905 / 6553600 bytes（37.2%）。WebRTC VAD 相对上一版人声增强固件增加约 336 B 静态 RAM 和约 9.0 KB Flash。
- 57 项主机测试通过，包含麦克风完整帧/CRC、重同步、边界校验和 WAV 元数据测试。
- COM3（ESP32-S3，USB `303A:1001`）烧录和校验通过。
- 设备报告 `mic state=ready`、`sample_rate=8000Hz`、`ring_ms=6000`、`monitor=off`；采样持续增长。启动后观察到 1 次 I²S read error，后续短测未增长，长期稳定性仍待验证。
- `mic-test.wav` 为 1 声道、16-bit、8000 Hz、24000 frames、3.000 秒；协议 CRC 通过。
- 安静环境样本统计为 min -106、max 99、mean -0.6、RMS 24.2，24000 个样本中 23612 个非零，排除全零/固定值假录音；本轮没有可控人声刺激，不能据此宣称语音清晰度已通过。
- 上述低幅度结果来自增加 ADC_SYNC 和可调增益前的首轮固件。重新烧录后，`normal` 安静环境录音为 min -1550、max 1639、RMS 207.5、0 个削波样本，实测比首轮 RMS 高约 18.7 dB，与理论增加 19.5 dB 基本一致。
- `high` 稳定后的安静环境录音为 min -6507、max 5147、RMS 603.0、0 个削波样本；切换后的第一段录音曾捕获到较大瞬态并出现 32 个满幅样本，因此 `high` 仍需用正常说话和近距离大声测试削波。
- 缓存回放在系统音量 60% 时较轻；该音量曲线实际为 -17 dB。用户把音量调至 100%（0 dB）后确认声音明显更清晰，说明采集、缓存和 DAC/功放链路均已工作，主要限制来自播放端衰减。暂不额外叠加数字回放增益，以免与 100% 音量及 `high` 输入档共同造成失真。
- 耳返命令短暂切换为 `on` 后成功恢复 `off`；500 ms 缓存回放状态可正常结束。采样增长到约 251 万帧时 read error 仍保持 1，未修改设备原有的 10% 持久化播放音量。
- 麦克风页面首屏和滚动底部均完成 320x240 真机截图，状态、电平条、耳返和回放项无重叠。
- 轻量降噪版本构建后 RAM 118256 / 327680 bytes（36.1%），Flash 2425429 / 6553600 bytes（37.0%）；相对增益版本只增加约 64 B 静态 RAM 和约 3.7 KB Flash（其中包含新增中文字形）。
- 以下两条是已替换的旧版自适应 expander 证据，其中 `noise_rms` 是旧实现自己的输入背景跟踪值，不是 Speex 内部噪声估计：主机合成序列中，稳定噪声阶段为 `noise_rms=173 / gain=20% / voice=no`，加入 440 Hz 语音替代信号后 7 帧内恢复到 `gain=99% / voice=yes`，信号结束后回到 20%。
- 旧版真机 A/B 时环境并非静音，而是 PC 音响持续播放音量不大的音乐。`denoise=off` 录音 RMS 314.74、P99 837；开启并稳定 15 秒后 RMS 75.34、P99 239，降低约 12.42 dB，0 个削波样本。设备同时报告 `noise_rms=227 / denoise_gain=30% / voice=no`，未再出现估计器向异常低值漂移。
- 独立 AGC 主机合成测试中，背景保持 100%，低幅语音达到 400%，语音结束后恢复 100%；25000 峰值输入被限制到 29999，限幅状态正确。
- 旧能量判据在低音量背景音乐下偶尔会把音乐瞬态当成人声；用户实测说话时增益约 200%，但音乐也有偶发增强。改用 WebRTC VAD 后，同一背景音乐下连续约 27 秒的 20 次状态采样均为 `voice=no / voice_gain=100%`；正常说话时 15 次采样中 5 次命中，增益为 222%～400%，停顿后恢复 100%。这证明本轮误触发显著收敛，但无喇叭参考的 VAD 仍不能区分近场真人、录制人声或音乐中的歌声。
- 麦克风页在 320×240 真机截图中显示降噪与 `vad wait / gain 100%` 动态值，七项滚动布局无重叠。设备保持输入增益 `normal`、降噪 `on`、人声增强 `on`、耳返 `off`。
- 按需采集版本启动时为 `capture=idle / frames=0`；进入麦克风页后约 0.8 秒达到 `capture=active / frames=6656`，Home 退出后恢复 `idle`，间隔 0.6 秒复查 `frames` 保持 10752 不再增长，RMS/Peak/Level 归零。
- `mic denoise on` 后等待合并写，日志出现 `[audio] settings saved`；使用 RTS 硬复位后仍报告 `denoise=on`，证明 `pgos_audio/mic_denoise` 持久化通过，同时启动采集仍为 `idle`。
- 从 idle 状态运行 `capture_microphone.py --duration 1000` 成功得到 8000 Hz mono PCM16LE 的 1000 ms WAV；工具结束后连续两次状态均为 `capture=idle / frames=10752`，证明 USB 临时采集已释放。
- SpeexDSP 最终版构建占用约 RAM 118.6 KB（36.2%）、Flash 2.456 MB（37.5%）；固定点频域处理、PSRAM 分配适配和新增中文字形均计入。原生合成测试确认稳定宽带噪声可压到输入 RMS 的 55% 以下，同时保留混合语音 RMS 的 55% 以上；AGC 释放及 3 帧/RMS 650 VAD 门控也有独立回归测试，完整主机套件为 60 项。
- 真机首次动态分配版本复现一次 `abort()`：符号化回溯落在 Wi-Fi 认证日志首次建立 newlib 递归锁，原因是 Speex 从内部 RAM 集中分配工作区后与 Wi-Fi 启动高峰争用。最终版改为启动阶段在 PSRAM 一次性分配并保持，真机启动日志报告 `SpeexDSP denoiser ready in PSRAM`。
- 最终版在 Wi-Fi 自动连接同时连续 8 次执行 `page mic → mic status → home`，每次均为页面内 `capture=active / ns=speex / vad=webrtc3`、页面外 `capture=idle / ns=idle`，无重启、read error 为 0；音频任务最低剩余栈约 4848 bytes。
- USB 最终处理链成功导出 5.000 秒 WAV，40000 个样本中 39668 个非零，peak 14318、0 个大于等于 30000 的削波样本。该样本只用于协议、非零和削波检查，环境与说话内容不可控，不作为主观清晰度结论。
- 最终 Speex + WebRTC VAD 链加入连续 3 帧和降噪后 RMS 650 准入门槛后，在 PC 音响持续播放同一低音量 BGM 的约 10 秒状态采样中 `voice=yes` 为 0/19；录音 RMS 325.1、peak 2040、P99 940、削波 0，设备状态为 `ns=speex / background_input_rms=272 / voice=no / voice_gain=100%`。BGM 内容随时间变化，因此幅值不用于和旧录音做严格 A/B，只把 0/19 作为误增强收敛证据。
- 最终提交候选重新烧录并校验后，USB 导出 5.000 秒、40000 样本的 WAV：39906 个非零，min -1086、max/peak 1205、RMS 250.5、削波 0。前台采集时回放最近 3 秒，状态显示 `capture_gate=closed / ns=unavailable / vad=unavailable`；回放结束恢复 `capture_gate=open / ns=speex / vad=webrtc3`。Home 后连续两次状态的 `frames` 均为 116480，`errors=0`，音频任务最低剩余栈 4856 bytes。

证据：

- [麦克风页面](../images/audio-microphone.png)
- 页面底部、降噪控件和 WebRTC VAD 控件的原始截图保留在本地 `captures/`，未纳入公共仓库。
- `captures/mic-test.wav`（本地真机录音，是否纳入 Git 由后续资产策略决定）
- `captures/mic-normal.wav`、`captures/mic-high-settled.wav`（可调增益对比录音）
- `captures/mic-music-denoise-off.wav`、`captures/mic-music-denoise-on.wav`（低音量背景音乐 A/B）

## 待验收边界

1. 用户只需对最终版做一次主观试听：正常说话后回放最近 3 秒，检查可懂度、背景残留、软音节开头和刺耳/泵动；体感不理想时再根据单次反馈微调 `-22 dB`、VAD mode 或 AGC 目标。
2. 近距离大声说话时确认 100% 播放音量下无明显爆音或失真。
3. 从低音量开始验证耳返，检查板载扬声器与麦克风之间的声学反馈；禁止直接高音量长开。AEC 需要喇叭 PCM 参考和独立的自适应滤波实验，不能用当前降噪开关代替。
4. 连续运行至少 30 分钟，确认 read error 不增长、界面/USB/Wi-Fi 不被音频任务阻塞。
5. 若后续增加网络音频，必须使用独立 BulkData 通道、取消/超时和背压，不能复用控制 TCP 字节流。
