# 实验 005：PGOS UI v2 与 LVGL 9.5

## 目标

借鉴 Peak 项目的页面生命周期、状态栏、焦点动画和资源组织经验，把 PGOS 当前的调试型 Sprite 界面升级为可持续扩展的 retained-mode UI，同时不改动 Wi-Fi、TCP、控制台和应用服务边界。

## 固件实现

- 环境：`playground`
- 图形库：`lvgl @ 9.5.0`
- 显示：LVGL 9 原生 `lv_display_create` / `lv_display_set_buffers`，40 行 RGB565 局部缓冲
- 内存：显示服务在 PSRAM 中维护 320×240 RGB565 shadow framebuffer（153600 bytes），用于截图和调试
- UI 运行时：`src/ui/UiRuntime.*`
- 当前页面：Launcher、System、Display、Network
- 新增导航命令：`home`，主机控制台快捷键为 `B`

## 视觉和交互

- 深色背景、暖橙强调色、灰色次要信息和统一的 22px 状态栏。
- 桌面使用图标卡片；当前焦点卡片会进行位置/宽度 overshoot 动画。
- 网络、系统页的状态卡在遥测刷新时不再重复播放焦点动画；动画只用于用户选择或页面切换。
- Wi-Fi 扫描使用列表行，SSID、RSSI 和选择状态在屏幕上显示；密码仍只通过隐藏 USB 控制台输入。
- 页面转场由 `UiRuntime` 统一处理，应用不直接触摸屏幕总线。

## 构建和真机验证

```powershell
pio run -e playground
pio run -e playground -t upload --upload-port COM3
python tools/capture_screen.py --port COM3 --output captures/pgos_ui_v2_final.png
```

构建结果（2026-07-18）：

- RAM：50400 bytes / 327680 bytes（15.4%）
- Flash：约 1.08 MB / 6.25 MB app 分区（16.4%）
- LVGL 9.5 编译、链接和烧录通过。

真机结果：

- 桌面、焦点移动、Display、System、Network 页面截图通过。
- Wi-Fi 仍为 `connected`，IP `192.168.1.6`，RSSI 约 `-16 dBm`，重连次数 0。
- TCP 目标 `192.168.1.4:19000` 配置保留，控制服务关闭时状态栏显示 `TCP OFF`。
- Wi-Fi 扫描页显示非阻塞扫描状态，扫描列表显示 SSID 和 RSSI。
- RGB565BE 截图协议继续通过，颜色和方向正确。

截图：

- [UI v2 桌面](../../captures/pgos_ui_v2_final.png)
- [UI v2 网络页](../../captures/pgos_ui_v2_network_final.png)
- [UI v2 Wi-Fi 扫描中](../../captures/pgos_ui_v2_wifi_scanning.png)
- [UI v2 Wi-Fi 列表](../../captures/pgos_ui_v2_wifi_selection.png)

## 经验和边界

1. LVGL 9 的显示接口与 8.x 不兼容；本项目使用 9.5 原生 API，不依赖 v8 兼容映射。
2. `lv_color_t` 在 LVGL 9 中不是 RGB565 像素类型，显示缓冲必须按 `LV_COLOR_FORMAT_RGB565` 的 2 字节布局处理。
3. 全屏 shadow framebuffer 保留了截图能力，但普通页面使用局部 flush，避免每次状态变化都推送 153600 bytes。
4. 当前 LVGL 页面使用内置 Montserrat ASCII/符号字体；Fusion Pixel BitmapFont 继续保留给未来游戏和中文字体实验，尚未把中文完整接入 LVGL 字体回退链。
5. 页面刷新和网络服务仍在同一主循环中，未复制 Peak 的高优先级独立 LVGL 任务；后续只有确实存在阻塞需求时才增加专用任务。
