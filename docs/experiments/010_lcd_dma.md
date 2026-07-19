# 实验 010：LCD 异步 DMA flush

> 日期：2026-07-19
> 状态：Arduino-ESP32 3.3.9 / ESP-IDF 5.5.4 原生 `esp_lcd` DMA 已真机验证通过

## 基线

System Monitor 真机观测到：

- 静止页面各阶段通常低于 1 ms。
- 页面动画偶发 `ui` 约 65 ms。
- 同一次大面积刷新中，LCD SPI 约 33 ms、shadow framebuffer 拷贝约 5 ms，
  LVGL 分成约 6～8 个区域 flush。
- 镜像发送阶段约 6～10 ms；镜像关闭时仍会出现约 67 ms 的 `ui` 峰值。

完整 320×240 RGB565 帧为 153600 字节。40 MHz SPI 的纯线速下限约 30.7 ms，
实测 33 ms 已接近物理总线极限。DMA 的目标不是缩短线速，而是让 SPI 传输与
LVGL 绘制、网络和其它主循环工作重叠。

## 本轮实现

### 3.3.9 / IDF 5.5.4 回归与迁移记录

- 升级到 pioarduino 55.03.39（Arduino-ESP32 3.3.9、ESP-IDF 5.5.4）后，旧
  TFT_eSPI 异步 DMA 路径曾出现实体 LCD 保留上一帧，而 shadow framebuffer 仍正确。
- 当前后端改为 Espressif Component Registry 的 `esp_lcd_ili9341` 2.0.2，使用
  SPI2/FSPI GPIO10/11/12/13、DC GPIO46、40MHz、事务队列深度 2 和
  `on_color_trans_done` 完成回调；不再直接操作 TFT_eSPI/IDF 私有 DMA 寄存器。
- LCD 初始化、横屏旋转（MADCTL MV+BGR）、反色、首帧和 Xbox 手柄连接均已在 COM3
  真机通过。CPU 从原先实际运行的 160MHz 恢复到项目硬件基线 240MHz 后，用户实测
  动画交互明显更流畅，部分转场约 32ms。

- LVGL 优先申请两块 320×40 RGB565 内部 DMA-capable draw buffer，每块 25.6 KB。
- 两块内部缓冲或 DMA 初始化失败时，自动回退到原有 PSRAM 单缓冲同步 flush。
- 原生 `esp_lcd` DMA 成功时启用 LVGL 双缓冲和 `flush_wait_cb`；DMA 回调记录真实
  完成时间，避免把主循环稍后处理回调的延迟误算进 SPI 线速。
- flush callback 启动 DMA 后立即返回；LVGL 需要复用缓冲时才等待上一次传输。
- shadow framebuffer 暂时保留，维持现有完整帧镜像和 USB 截图协议。
- 原生 DMA flush 已真机确认：一个 draw buffer 由 DMA 发送时，LVGL 可以使用另一个
  buffer 渲染下一块区域；一帧最后一次 DMA 完成后再释放刷新状态。运行时同时输出
  `wait=`，用于区分实际 DMA 等待和 LVGL 软件渲染时间。
- shadow 只在镜像 socket 已连接或截图明确请求时更新；镜像关闭时 copy 阶段退出
  显示热路径。新镜像连接会强制一次全屏 LVGL redraw，完成 keyframe 后才发送。

## shadow framebuffer 的后续边界

LVGL 和 ILI9341 GRAM 已经负责正常显示的保留式刷新，shadow 不再用于防闪屏。
当前保留它仅因为镜像协议发送完整 RGB565 帧。

后续计划：

1. 镜像改为首次 keyframe 加 dirty rectangles，由 Studio 合成完整 framebuffer。
2. 按需截图启用 LVGL 9.5 `lv_snapshot_take_to_draw_buf()`；当前状态栏位于
   `lv_layer_top()`，需要调整层级或单独合成。
3. 完成协议迁移后删除常驻 shadow 及其约 5 ms 的大面积复制成本。

## 验证

- `pio run -e playground`：通过。
- `python -m unittest discover tools\\tests`：13 项通过。
- 当前 240MHz 构建资源：RAM 75808 / 327680 bytes（23.1%）；Flash 1691713 /
  6553600 bytes（25.8%）。
- 启动日志出现 `[display] backend=esp_lcd ... clock=40MHz` 和
  `[display] async flush enabled`。
- 旧 TFT_eSPI 路径下，shadow/网络镜像可以持续更新但实体 LCD 只保留旧 GRAM 画面；
  原生 `esp_lcd` 迁移后实体屏恢复刷新，交互肉眼可见更流畅。
- SPI 仍受 40MHz 物理线速限制；`flush_us spi` 是一轮 LVGL 所有 dirty area 的总和，
  不是单次传输。`pixels` 接近一整屏时约 30.7ms 属于正常线速，`wait` 很低而 `ui`
  较高时，瓶颈在 LVGL 软件重绘而不是 DMA。
- 页面颜色、区域刷新、进入/返回动画与状态栏均正常。
- 镜像关闭时动画 `copy` 稳定为约 0 ms；镜像开启时自动生成当前画面的完整
  keyframe，USB 按需截图也能捕获当前画面。

## SPI 高频试验边界

Peak README 的 60Hz 估算针对 240×240，而它实际的 `Setup24_ST7789.h` 同样使用
40MHz。本板为 320×240，整帧 RGB565 在 40MHz 下的线速下限约 30.72ms；整帧
60Hz 需要约 73.7MHz。

2026-07-19 曾增加临时 A/B 环境，同时把总线从 HSPI 切到 GPIO10～13 对应的 FSPI，
并把写时钟提高到 80MHz。固件可以构建和烧录，但实体屏出现花屏并停止刷新，随后
立即恢复默认 40MHz HSPI 固件。由于该试验同时改变了总线与时钟，不能单独归因于
其中一个变量；结论仅是“FSPI + 80MHz”组合在当前样机不稳定。后续若继续试验，
必须分别测试 FSPI/40MHz 或 HSPI/较低增量频率，并始终保留 40MHz 锚点。
