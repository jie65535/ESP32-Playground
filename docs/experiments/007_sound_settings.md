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
- 现场试听确认 40% 已很轻，60% 适合作为日常音量，100% 明显很响；硬件播放链路成立，但 UI 百分比到听感的曲线尚未冻结。
- RAM：51208 bytes / 327680 bytes（15.6%）。
- Flash：1126521 bytes / 6553600 bytes（17.2%）。

Android 的公开音频策略不是把 UI 百分比线性映射到振幅，而是把 UI index 通过分段曲线转换成 dB。AOSP 默认扬声器媒体曲线的参考点为 `1 → -58 dB`、`20 → -40 dB`、`60 → -17 dB`、`100 → 0 dB`，并允许不同音频用途和输出设备使用不同曲线：

- <https://source.android.com/docs/core/audio/implement-policy>
- <https://android.googlesource.com/platform/frameworks/av/+/refs/heads/main/services/audiopolicy/config/default_volume_tables.xml>

因此 40% 听起来远小于“最大响度的一半”是正常现象。PGOS 当前仍把 UI 百分比线性写入 ES8311 音量寄存器；下一步应根据这块扬声器的实听结果建立分段 dB/寄存器表，而不是盲目把 40% 拉平到很响。

截图：

- [Sound 设置页](../../captures/pgos_sound_settings.png)
- [焦点居中的 Sound 列表](../../captures/pgos_sound_title_scroll.png)
- [滚动桌面与 Wi-Fi 波纹状态图标](../../captures/pgos_ui_scroll_wifi_wave.png)

## 后续边界

音乐播放、音频文件、麦克风录音和音效资源不在本实验；它们需要独立的 AudioService 流式接口、文件/资源预算和停止/取消状态机。网络镜像或 TCP 投屏也不应把音频 DMA 和 LVGL 页面线程绑在一起。
