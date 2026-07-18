# 实验 010：LCD 异步 DMA flush

> 日期：2026-07-18  
> 状态：代码与构建通过，等待真机验证

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

- LVGL 优先申请两块 320×40 RGB565 内部 DMA-capable draw buffer，每块 25.6 KB。
- 两块内部缓冲或 DMA 初始化失败时，自动回退到原有 PSRAM 单缓冲同步 flush。
- TFT_eSPI DMA 成功时启用 LVGL 双缓冲和 `flush_wait_cb`。
- flush callback 启动 DMA 后立即返回；LVGL 需要复用缓冲时才等待上一次传输。
- 最后一块区域的 DMA 在后续主循环中轮询完成，再通知 LVGL flush ready。
- shadow framebuffer 暂时保留，维持现有完整帧镜像和 USB 截图协议。

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
- 构建资源：RAM 52292 / 327680 bytes（16.0%）；Flash 1153229 / 6553600 bytes（17.6%）。

## 真机待测

- 启动日志应出现 `[display] SPI DMA enabled` 和 `[display] async flush enabled`。
- 验证颜色、区域刷新、页面进出动画和状态栏均无错色、撕裂或冻结。
- 比较 DMA 前后的 `ui`、SPI、copy、main-loop duty 和镜像 FPS。
- 若 DMA 初始化或内部缓冲失败，应自动显示同步回退日志并保持界面可用。
