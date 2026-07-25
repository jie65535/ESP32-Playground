# 实验 007：声音设置与 ES8311 最小播放

## 目标

复用已在 BatPanel 同型号板上验证的 ES8311/I²S 方向，在 PGOS 中增加一个可降级的声音设置应用。第一版不做音乐播放器，只验证音量、短交互反馈音和人工试听。

## 硬件与实现

- ES8311 I²C 地址：`0x18`，控制总线 GPIO15/16。
- ESP32 播放数据：I²S GPIO8；MCLK/BCLK/WS 为 GPIO4/5/7。
- 功放使能：GPIO1，低电平开启；初始化期间保持关闭。
- I²S：8 kHz、16-bit、双声道，固定 1 kHz 方波作为最小可验证音。
- 音频任务：独立 FreeRTOS 任务持续填充 DMA；没有反馈音时写入静音 PCM。
- NVS namespace：`pgos_audio`，保存 `volume` 和 `feedback`。
- 默认值：音量 60%，交互反馈音关闭；试听只由用户主动触发。

## 页面功能

- Volume：0%～100%，10% 步进。
- Feedback sound：开启/关闭短导航反馈音。
- Test tone：主动播放约 300 ms 的测试音。
- 音频初始化失败时显示 `Unavailable`，不影响显示、网络或其它页面。

## 真机验证

```powershell
pio run -e playground
pio run -e playground -t upload --upload-port COM3
python tools/playground_console.py --port COM3 --command "page sound"
```

真机结果：

- COM3 启动状态为 `audio state=ready volume=60% feedback=off`，ES8311/I²S 初始化成功。
- Test tone 能触发约 300 ms 播放，交互反馈音开关可以持久化。
- 现场试听确认旧曲线下 40% 已很轻，60% 适合作为日常音量，100% 明显很响；硬件播放链路成立，当前已部署临时手机式曲线，最终校准仍待继续试听。
- RAM：51208 bytes / 327680 bytes（15.6%）。
- Flash：1126865 bytes / 6553600 bytes（17.2%）。

Android 的公开音频策略不是把 UI 百分比线性映射到振幅，而是把 UI index 通过分段曲线转换成 dB。AOSP 默认扬声器媒体曲线的参考点为 `1 → -58 dB`、`20 → -40 dB`、`60 → -17 dB`、`100 → 0 dB`，并允许不同音频用途和输出设备使用不同曲线：

- <https://source.android.com/docs/core/audio/implement-policy>
- <https://android.googlesource.com/platform/frameworks/av/+/refs/heads/main/services/audiopolicy/config/default_volume_tables.xml>

ES8311 数据手册进一步确认，DAC 音量寄存器 `0x32` 以 `0.5 dB/step` 调节：`0x00 = -95.5 dB`、`0xBF = 0 dB`、`0xFF = +32 dB`。旧实现把 0%～100% 线性映射到 `0x00`～`0xFF`，等于让 100% 使用 `+32 dB` 数字增益；这解释了 40% 几乎听不清、60% 合适而 100% 突然非常响的现象。

当前先采用“小扬声器校准曲线”：`1% = -41 dB`、`10% = -35 dB`、`20% = -32 dB`、`30% = -25 dB`、`40% = -22 dB`、`50% = -19 dB`、`60% = -17 dB`、`100% = 0 dB`，区间线性插值，0% 单独硬静音。根据首次试听结果只抬高 1%～10% 区间，避免最低音量落入听感/噪声底；该曲线仍是实验性校准，后续以实际试听为准。

截图：

- [Sound 设置页](../../captures/pgos_sound_settings.png)
- [焦点居中的 Sound 列表](../../captures/pgos_sound_title_scroll.png)
- [滚动桌面与 Wi-Fi 波纹状态图标](../../captures/pgos_ui_scroll_wifi_wave.png)

## 后续边界

音乐播放、音频文件和音效资源不在本实验；麦克风录音已在[实验 025](025_microphone_capture.md)单独建立 AudioService 缓冲和 USB 导出边界。网络镜像或 TCP 投屏也不应把音频 DMA 和 LVGL 页面线程绑在一起。
