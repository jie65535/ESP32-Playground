# 实验 011：板载 RGB LED 灯光应用

> 日期：2026-07-19  
> 状态：固件烧录、板载 LED 与 NVS 重启恢复已验证；平滑曲线待用户复测

## 目标

- 验证 ES3N28P 板载 WS2812 与 GPIO42 基线。
- 把 RGB 做成独立系统服务，而不是在页面或 `main` 中直接写时序。
- 灯效必须非阻塞，不影响 LVGL、Wi-Fi、镜像或音频。
- 提供一个符合现有手机式导航的可滚动 RGB Light App。

## 实现

`RgbService` 使用 Arduino-ESP32 自带的 RMT `neopixelWrite()` 发送单颗 WS2812 的
GRB 数据，不增加第三方库。一次发送约 30µs；颜色没有变化时不重复发送。

默认行为：

- 上电关闭，避免启动时产生强光。
- 默认效果为 Rainbow、亮度 40%、速度 Normal。
- Power 只在当前运行会话中保存，重启后始终关闭。
- Effect、Palette、Brightness 和 Speed 保存到 `pgos_rgb` NVS namespace，连续操作停止 750ms 后合并写入。
- 离开 RGB Light 页面后灯效继续运行；服务生命周期不依赖页面生命周期。

页面提供五个设置：

1. Power：灯光开关。
2. Effect：Solid、Breathe、Rainbow、Heartbeat、Sparkle。
3. Palette：Cyan、Violet、Rose、Amber、Green、Blue、White。
4. Brightness：5%～100%，以常用的 5%～10% 步进调整。
5. Speed：Slow、Normal、Fast。

上下移动焦点，左右调整，回车执行当前设置；Backspace/Back 返回桌面。呼吸、彩虹
和心跳使用 `millis()` 相位推进，Sparkle 使用有界伪随机亮度，不调用长时间 `delay()`。
亮度修改采用目标值/实际值分离，LED 在约 300ms 内渐变到新目标，避免按键调整时突然跳变；
呼吸和心跳使用 smoothstep 曲线，并以 10ms 节拍刷新，避免在线性三角波的峰值出现速度突变。

## 构建与测试

```powershell
pio run -e playground
python -m unittest discover -s tools/tests -q
python -m py_compile tools/playground_console.py tools/pgos_studio.py
```

当前构建结果：

- RAM：53308 / 327680 bytes（16.3%）。
- Flash：1170989 / 6553600 bytes（17.9%）。
- Python：13 项测试通过。

烧录和打开页面：

```powershell
pio run -e playground -t upload --upload-port COM3
python tools/playground_console.py --port COM3 --command "page rgb"
```

## 真机验证

- GPIO42/RMT 能实际驱动板载 WS2812；用户已观察到灯光和不同亮度变化。
- 第一版二次三角波的亮度渐变被用户判断为不够丝滑；`3073e00` 改为 10ms
  smoothstep 呼吸/心跳曲线并已烧录，最终观感待下次继续确认。
- `3a52924` 增加 `pgos_rgb` 持久化。真机设置为
  `Breathe / Violet / 50% / Fast`，等待 750ms 后重新烧录/重启，状态完整恢复。
- 同一次重启后 `state=off`、RGB 输出为 `0,0,0`，证明 Power 没有被持久化。
- 验证时 Wi-Fi 与 ServerService 仍保持连接，未观察到 RGB 更新导致链路失效。

## 仍待观察

- 七种调色板的 RGB 顺序是否正确，尤其是红/绿是否互换。
- 10%～100% 亮度是否有足够可用范围，100% 白色是否引起供电或 Wi-Fi 异常。
- smoothstep 后五种灯效在 Slow/Normal/Fast 下是否连续、无明显阶梯感。
- 灯效运行时页面动画、无线控制、镜像和音频是否仍正常。

真机确认后再决定是否增加通知灯语义、音乐律动或从上位机推送灯光场景。
