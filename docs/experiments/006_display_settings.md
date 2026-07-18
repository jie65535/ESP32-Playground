# 实验 006：显示亮度与自动息屏

## 目标

为 PlaygroundOS 增加独立 Display Settings 应用，允许调整屏幕亮度和空闲息屏时间。设置必须持久化，息屏不能阻塞 LVGL、USB、Wi-Fi 或后台服务，用户输入后能够立即唤醒。

## 实现

- 背光引脚：GPIO45，高电平点亮。
- PWM：Arduino LEDC，5 kHz、8 bit、channel 0。
- 亮度档位：20%、40%、60%、80%、100%。
- 息屏档位：Never、15 秒、30 秒、1 分钟、5 分钟、15 分钟。
- 持久化：Preferences namespace `pgos_display`，键为 `brightness` 和 `timeout_s`。
- 写入策略：设置变化后延迟 750 ms 合并写入，避免焦点刷新或连续按键反复写 Flash。
- 息屏语义：只把背光 PWM duty 设为 0，LVGL 渲染、shadow framebuffer、USB、Wi-Fi 和网络服务继续运行。
- 唤醒语义：第一次方向/确认/返回输入只点亮背光，不继续执行页面操作；状态、帮助和截图等只读命令不会强制唤醒。

## 应用交互

Display Settings 页面包含两个普通设置项：

- Brightness：显示百分比和亮度条。
- Screen timeout：显示当前空闲时间档位。

页面不显示按键教程。上下移动设置项，左右调整当前值；Enter 等价于向前切换当前设置。Backspace 使用系统统一返回语义。

## 构建与真机结果

```powershell
pio run -e playground
pio run -e playground -t upload --upload-port COM3
python tools/playground_console.py --port COM3 --command "page settings"
```

2026-07-18 实测：

- RAM：50632 bytes / 327680 bytes（15.5%）。
- Flash：1089725 bytes / 6553600 bytes（16.6%）。
- 亮度从 100% 调整到 80% 后背光即时变化。
- 息屏设为 15 秒后，状态由 `backlight=on` 自动变为 `backlight=off`。
- 第一枚方向键仅唤醒，设置值保持不变；第二枚方向键才修改设置。
- 设置为 80% / 15 秒并重新烧录复位后，NVS 正确恢复相同值。
- 验证结束后已恢复为 100% / Never。

截图：

- [Display Settings](../../captures/pgos_display_settings.png)
- [包含四个应用的 Launcher](../../captures/pgos_launcher_four_apps.png)

## 边界

当前“息屏”是背光关闭，不会发送 ILI9341V sleep command；因此唤醒快且不会影响 framebuffer，但 LCD 控制器本身仍保持工作。后续做低功耗实验时，再单独评估 LCD sleep、Wi-Fi 省电和 ESP32 light sleep，不能把它们混入本次 UI 设置。
