# 实验 005：PGOS UI v2 与 LVGL 9.5

## 目标

借鉴 Peak 项目的页面生命周期、状态栏、焦点动画和资源组织经验，把 PGOS 当前的调试型 Sprite 界面升级为可持续扩展的 retained-mode UI，同时不改动 Wi-Fi、TCP、控制台和应用服务边界。

## 固件实现

- 环境：`playground`
- 图形库：[`lvgl @ 9.5.0`](https://github.com/lvgl/lvgl)
- 显示：LVGL 9 原生 `lv_display_create` / `lv_display_set_buffers`，40 行 RGB565 局部缓冲
- 内存：显示服务在 PSRAM 中维护 320×240 RGB565 shadow framebuffer（153600 bytes），用于截图和调试
- UI 运行时：`src/ui/UiRuntime.*`
- 当前页面：Launcher、System、Display、Network
- 新增导航命令：`back` / `home`，Backspace 映射为 `back`

## 视觉和交互

- 深色背景、暖橙强调色、灰色次要信息和统一的 22px 状态栏。
- 桌面使用图标卡片；当前焦点卡片会进行位置/宽度 overshoot 动画。
- 网络、系统页的状态卡在遥测刷新时不再重复播放焦点动画；动画只用于用户选择或页面切换。
- 导航采用手机式分层：桌面方向键移动应用选择，Enter 进入；应用内方向键由当前应用处理，Backspace 返回桌面。
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
- 分层导航通过：Right+Enter 进入 Color Lab，应用内 Down 不会切换到下一应用，Back 返回桌面。
- Wi-Fi 仍为 `connected`，IP `192.168.1.6`，RSSI 约 `-16 dBm`，重连次数 0。
- TCP 目标 `192.168.1.4:19000` 配置保留，控制服务关闭时状态栏显示 `TCP OFF`。
- Wi-Fi 扫描页显示非阻塞扫描状态，扫描列表显示 SSID 和 RSSI。
- RGB565BE 截图协议继续通过，颜色和方向正确。

2026-07-19 增量验证：

- 新增 `LvglBitmapFont`，通过 LVGL 9.5 的 `get_glyph_dsc` /
  `get_glyph_bitmap` 回调直接复用 `BitmapFontData.h` 中的 Fusion Pixel
  1bpp 字形，不复制第二份字库。
- Color Lab 页面底部加入“屏幕测试 中文”作为第一处 LVGL 中文像素字体入口；
  COM3 构建、烧录和页面运行通过，设备状态保持 `app=Display`，空闲
  `ui` 约 8 us，LCD 仍为 `backend=esp_lcd dma=on async=on`。
- RAM 75792 / 327680 bytes，Flash 1704481 / 6553600 bytes。字库首次被
  当前 UI 引用后约增加 23 KB Flash；运行时不复制完整字库到 RAM。
- 本次 USB 截图在二进制帧末尾分别缺少 117/41 bytes，主机超时关闭串口后
  设备仍有未发送完的截图数据；硬复位后控制台和页面恢复正常。中文实体屏
  视觉结果仍需人工确认，不能把本次截图记为通过。
- 随后发现验证文案中的“中文”不在旧的手工字符清单中；源码扫描自动发现
  `中`、`文`，本地 BDF 自动补齐后重新编译、烧录，14 项主机测试通过。

截图：

- [PGOS 桌面](../images/pgos-home.png)
- 网络页、Wi-Fi 扫描和列表截图保留在本地 `captures/`，未纳入公共仓库。

## 经验和边界

1. LVGL 9 的显示接口与 8.x 不兼容；本项目使用 9.5 原生 API，不依赖 v8 兼容映射。
2. `lv_color_t` 在 LVGL 9 中不是 RGB565 像素类型，显示缓冲必须按 `LV_COLOR_FORMAT_RGB565` 的 2 字节布局处理。
3. 全屏 shadow framebuffer 保留了截图能力，但普通页面使用局部 flush，避免每次状态变化都推送 153600 bytes。
4. Fusion Pixel BitmapFont 已提供 LVGL `lv_font_t` 适配，但当前只在 Color Lab
   做最小中文验证；其它页面仍主要使用 Montserrat，不能把适配层可用等同于
   全系统中文迁移完成。
5. 字符子集由 `tools/font_chars.txt` 与 `src/` C/C++ 字符串字面量共同决定，
   PlatformIO 构建前脚本负责检测和补齐；BDF 仍是本地生成源，不进入普通 Git 历史。
5. 页面刷新和网络服务仍在同一主循环中，未复制 Peak 的高优先级独立 LVGL 任务；后续只有确实存在阻塞需求时才增加专用任务。
