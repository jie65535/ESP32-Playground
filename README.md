# ESP32 Playground

面向 QD 电子 ES3N28P / ESP32-S3 R8N16 开发板的个人实验项目。

未来的可扩展设备基座命名为 PlaygroundOS（PGOS）：用统一的应用、服务、事件和连接抽象承载显示、Wi‑Fi、BLE、音频、网络控制台和小游戏实验。

这是一个面向 ESP32-S3 开发板的个人实验空间。目标是用小步实验把显示、USB、GPIO、RGB、音频、I²C、Wi-Fi、局域网通信和其它外设逐个玩明白，并把真实接线和实测结果记录下来。

当前固件点亮 ILI9341 屏幕，显示芯片、内存、运行时间和板卡信息，并提供设备端 Wi-Fi Station 配网：Network 页面可扫描/选择 2.4 GHz 网络，通过方向键软键盘输入密码并保存到设备 NVS；USB 控制台继续作为隐藏输入和维护后备，连接状态、IP 和 RSSI 同时显示在屏幕与控制台。

当前固件使用 LVGL 9.5 retained-mode UI：桌面收敛为“游戏 / 设置 / 系统工具”三级菜单，Back 返回上一级，Home 直接回桌面。Shell、设置页和系统工具已使用 Fusion Pixel 精简中文字库汉化，24px 页面标题和固定大字号游戏 HUD 保留英文；卡片图标使用独立的 Lucide A8 资源，不再绑定 LVGL 内置符号。Games 菜单包含 Snake、Tetris、Breakout 和 Blackjack：前三款继续保存 Top 5，Blackjack 使用独立规则引擎、筹码资料、发牌/翻牌动画和破产补助。时间页通过共享 GPIO15/16 I²C 总线读取 PCF8563 电池时钟；联网后自动以 SNTP 校时并写回 RTC，设置时间也可使用 USB 的 `time set YYYY-MM-DD HH:MM:SS` 命令；状态栏右上角显示有效时间源的 24 小时制 `HH:MM`，回退顺序为 NTP（本次开机已校时）→ PCF8563 → `--:--`。状态栏同时显示 Wi-Fi、服务端和蓝牙手柄状态。Controller 页面显示 Xbox BLE 手柄的连接、型号、电量和扫描倒计时，启动或用户请求时开放 60 秒配对扫描；连接后立即停扫，断线后通过 10 秒扫描 / 20 秒暂停的低占空比窗口自动重连，主动或空闲断开先等待 30 秒；空闲断开可设为 Never / 5 / 15 / 30 分钟并持久化。显示设置支持 GPIO45 PWM 亮度、空闲息屏、活动唤醒和 NVS 持久化；声音页支持 ES8311 音量、反馈音和试听；RGB Light 通过 GPIO42 非阻塞驱动板载 WS2812，提供静态、呼吸、彩虹、心跳和闪烁灯效。LVGL 使用 40 行 RGB565 局部缓冲，显示服务在 PSRAM 中维护完整 shadow framebuffer，因此仍保留无损截图与无线镜像能力；USB 状态日志只低频输出。

## 当前环境

Games 菜单中的 Platformer 已扩展为 1-1 至 8-4 共 32 关战役：USB 可使用 `page platformer` 或快捷键 `P` 直达；标题页默认选中 NVS 继续点，左右选择 WORLD/STAGE 字段、上下调整 1–8/1–4、`ok`/A 开始所选关卡。游戏中 `ok`/A 跳跃，X 暂停，B/Home 返回；关卡地图、区域、水管、藤蔓、平台、火焰棒和敌人出生点由离线工具转换成紧凑 C++ 数据，设备端不解析 PNG/XML/CSV。运行时使用固定池和一块 PSRAM RGB565 游戏画布，NVS 只保存继续关卡、通关状态和最高分；`platformer maptest [world-stage]` 可直达指定关卡，`platformer mapnext` 再逐段巡检原始地图。

- PlatformIO + Arduino
- `pioarduino/platform-espressif32 55.03.39`
- `Arduino-ESP32 3.3.9` / `ESP-IDF 5.5.4`
- `es3n28p_r8n16`
- `lvgl @ 9.5.0`
- ESP32-S3，16MB QIO Flash，8MB OPI PSRAM
- ILI9341V，逻辑分辨率 320×240 横屏
- 原生 USB CDC，`Serial` 波特率 115200

## 第一个固件

```powershell
pio run -e playground
pio run -e playground -t upload --upload-port COMx
pio device monitor --port COMx --baud 115200
```

当前固件不连接外部业务模块；Wi-Fi 凭据只保存在设备 NVS，不进入 Git。USB 输出启动信息、Wi-Fi 状态和低频运行日志。

## USB 控制台和截图

安装依赖后可使用箭头键、回车、快捷键和无损截图：

```powershell
python -m pip install -r tools/requirements.txt
python tools/playground_console.py --list
python tools/playground_console.py --port COM3
python tools/playground_console.py --port COM3 --wifi-setup
python tools/capture_screen.py --port COM3 --output captures/home.png
```

USB 调试控制台的显式 `page system`、`page time`、`page display`、`page settings`、`page sound`、`page rgb`、`page controller`、`page console`、`page network`、`page snake`、`page tetris`、`page breakout`、`page platformer` 和 `page blackjack` 可直达页面，Backspace 返回上一级，`home` 直接返回桌面；`C` 显示色卡，`R` 查询状态，`W` 启动 Wi-Fi 配网，`S` 会暂停日志读取线程并导出 LVGL shadow framebuffer 的原始 RGB565 画布到 PNG/Windows 剪贴板。四方向键移动当前页面焦点，回车确认，`Q` 退出控制台；`T`、`B`、`P` 和 `J` 分别直达俄罗斯方块、打砖块、超级马里奥和二十一点。固件也接受 `gamepad status`、`gamepad scan`、`gamepad stop`、`gamepad rumble`、`gamepad disconnect`、`back`、`home`、`time status`、`time set YYYY-MM-DD HH:MM:SS` 和 Wi-Fi 配置命令；密码不会由固件或控制台回显。

设备端进入“设置 → 无线网络 → 配置网络”即可启动同一套非阻塞扫描。加密网络会进入 320×240 方向键软键盘；方向键移动、确认输入，Xbox `X` 可快速切换字母/符号页，Back 返回网络列表。开放网络会跳过密码页直接保存并连接。

## PGOS Studio 无线上位机

PGOS Studio 是面向 PlaygroundOS 的轻量设备镜像与遥控器，定位类似开发板专用的精简 scrcpy：上位机监听控制连接，设备经 Wi-Fi 主动连接；GUI 只提供四方向、确认、返回和 Home，不维护设备页面深链接。页面直达命令仍保留在 USB/协议调试层，方便自动测试。

安装并启动：

```powershell
python -m pip install -r tools/requirements-studio.txt
python tools/pgos_studio.py --listen 0.0.0.0 --port 19000
```

首次通过 USB 给设备保存电脑地址；`server set` 会同时启用自动连接，之后 Wi-Fi 恢复或上位机重启时设备会非阻塞重连：

```text
server set 192.168.1.4 19000
server status
```

默认端口分工为：19000 控制与状态、19001 吞吐实验、19002 屏幕镜像。Studio 会显示控制 RTT、镜像实时 FPS/应用层 Mbps，并可直接发起 1/4/16 MiB 的设备上行/下行吞吐测试；测最大带宽时应先停止镜像。Studio 的“开始镜像”会通过控制协议临时启用设备 `MirrorService`，设备把 320×240 RGB565 shadow framebuffer 发送到独立镜像通道；该开关不写入 NVS，也不出现在设备设置菜单。当前是约 5 FPS 的完整原始帧 MVP，后续再增加脏矩形、录制、反向画面推送和音频。

无依赖的命令行诊断服务器仍保留，用于协议和自动化测试：

```powershell
python tools/pgos_server.py --listen 0.0.0.0 --port 19000
```

诊断服务器连接后可直接输入 `up`、`down`、`left`、`right`、`ok`、`back`、`home`、`status` 或调试用 `page ...` 命令。菜单中的四方向键只移动当前层选择，进入应用后由当前应用自行处理；`back` 返回父菜单，`home` 直接回到桌面。设备返回 ACK；`status` 还会返回结构化 STATE。Wi-Fi 密码、清除凭据和服务器配置等敏感命令不会通过 TCP 执行。

## 文档入口

- [项目交接与工作规则](AGENTS.md)
- [硬件和引脚基线](docs/HARDWARE.md)
- [项目路线](docs/PROJECT_PLAN.md)
- [微型应用平台架构](docs/ARCHITECTURE.md)
- [Wi-Fi 与局域网方案](docs/WIFI_PLAN.md)
- [实验记录：屏幕和板卡信息](docs/experiments/001_display_info.md)
- [实验记录：Wi-Fi Station](docs/experiments/002_wifi_station.md)
- [实验记录：TCP 诊断服务器](docs/experiments/003_tcp_server.md)
- [实验记录：TCP 应用层吞吐](docs/experiments/004_tcp_throughput.md)
- [实验记录：PGOS UI v2 / LVGL 9.5](docs/experiments/005_ui_v2_lvgl.md)
- [实验记录：显示亮度与自动息屏](docs/experiments/006_display_settings.md)
- [实验记录：声音设置与 ES8311 最小播放](docs/experiments/007_sound_settings.md)
- [实验记录：PGOS Studio 与无线屏幕镜像](docs/experiments/008_pgos_studio_mirror.md)
- [实验记录：运行时监视与 TCP 命令队列](docs/experiments/009_runtime_monitor.md)
- [实验记录：LCD 异步 DMA flush](docs/experiments/010_lcd_dma.md)
- [实验记录：板载 RGB LED](docs/experiments/011_rgb_led.md)
- [实验记录：PCF8563 I²C 实时时钟](docs/experiments/012_pcf8563_rtc.md)
- [实验记录：Xbox Series 手柄 BLE HID 可行性](docs/experiments/013_ble_xbox_gamepad.md)
- [实验记录：PGOS 贪吃蛇小游戏](docs/experiments/014_snake_game.md)
- [实验记录：PGOS 俄罗斯方块小游戏](docs/experiments/015_tetris_game.md)
- [实验记录：PGOS 打砖块小游戏](docs/experiments/016_breakout_game.md)
- [实验记录：PGOS 超级马里奥 1-1](docs/experiments/020_platformer_game.md)
- [实验记录：PGOS 超级马里奥 32 关战役](docs/experiments/021_platformer_campaign.md)
- [实验记录：Shell 架构收敛、图标与汉化](docs/experiments/017_shell_architecture_localization.md)
- [实验记录：PGOS 二十一点与事件动画](docs/experiments/018_blackjack_game.md)
- [实验记录：设备端 Wi-Fi 扫描与方向键软键盘](docs/experiments/019_on_device_wifi_setup.md)
- [可复用经验知识库](docs/knowledge/README.md)
- [第三方依赖与源码策略](docs/DEPENDENCIES.md)
- [厂商原始资料说明](docs/vendor/README.md)

原始厂商资料包位于 `docs/vendor/2.8inch_IPS_ESP32-S3_ILI9341V_ES3C28P_ES3N28P_V1.0`。该目录是参考资料，不是程序依赖；缺失时不应复制或修改其内容。
