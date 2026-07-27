# ESP32 Playground 新对话交接说明

> 最后更新：2026-07-26
> 工作目录：`G:\MCU\ESP32Playground`
> 当前阶段：PGOS 应用基座、中文大语言模型、LCD DMA、PGOS Studio、板载 RGB、PCF8563 RTC 和小游戏真机验收

## 1. 项目定位

ESP32 Playground 是针对 QD 电子 ES3N28P 无触摸版 ESP32-S3 开发板的个人实验项目。目标是用小步、可验证的实验逐项探索显示、输入、USB、GPIO、RGB、音频、I²C、Wi-Fi、BLE 和局域网通信，并把接线、测试和真机结论记录到 `docs/experiments/`。

PlaygroundOS（PGOS）是逐步形成的应用基座：系统服务拥有硬件和连接状态，前台 App 通过稳定接口使用能力，同一时刻只运行一个前台应用。

## 2. 新任务必读顺序

开始任务前按需阅读：

1. `README.md`
2. `docs/HARDWARE.md`
3. `docs/PROJECT_PLAN.md`
4. 涉及平台或模块边界时阅读 `docs/ARCHITECTURE.md`
5. 涉及网络时阅读 `docs/WIFI_PLAN.md`
6. 需要复用经验时阅读 `docs/knowledge/README.md` 和对应条目
7. 涉及现有功能时阅读对应的 `docs/experiments/*.md`
8. 需要厂商原文时才进入 `docs/vendor/2.8inch_IPS_ESP32-S3_ILI9341V_ES3C28P_ES3N28P_V1.0`

发生冲突时遵循以下优先级：

- `docs/HARDWARE.md` 是项目采用的引脚基线。
- 厂商原始资料是器件和电路事实来源。
- 实验记录的“实测”只对记录日期、硬件和 Git 提交负责，不自动升级为固定规格。
- 注释、原理图和真机不一致时，记录差异，以原理图和真机为准。

## 3. 硬件基线

| 功能 | 当前配置 |
|---|---|
| 主控 | ESP32-S3 R8N16，16MB QIO Flash，8MB OPI PSRAM |
| 屏幕 | ILI9341V；物理 240×320，固件使用 320×240 横屏 |
| LCD | CS 10、MOSI 11、SCK 12、MISO 13、DC 46、背光 45 |
| USB / RGB | 原生 USB CDC：19/20；WS2812：42 |
| 音频 | ES8311；I²C 15/16；I²S MCLK/BCLK/WS/DAC：4/5/7/8；功放 EN：1，低电平使能 |
| RTC | PCF8563，7-bit 地址 `0x51`，与 ES8311 共用 GPIO15/16 |
| 保留脚 | BOOT GPIO0 仅用于下载/启动；麦克风 ADC DATA 使用 GPIO6 |

完整引脚、电气边界和扩展脚见 [`docs/HARDWARE.md`](docs/HARDWARE.md)。外接电源、串口电平、扬声器、麦克风和 RTC 模块都必须先确认 3.3V 兼容、共地和电池安全。

## 4. 当前实现摘要

- PlatformIO 环境为 `playground`；固件入口在 `main/`，由 `SystemKernel`、`AppManager`、Services 和前台 Apps 分层编排。
- DisplayService 使用原生 `esp_lcd_ili9341`、SPI2/40MHz 和 DMA；LVGL 9.5 使用内部 DMA 缓冲。80MHz 方案曾导致花屏，未经单变量实验不得恢复。
- UiRuntime 统一拥有 LVGL、主题、状态栏、菜单历史和页面转场；桌面为 Games / Settings / System Tools 三级菜单，Back 逐级返回，Home 回桌面。
- 当前服务包括 Display、Input、Wi-Fi、BLE Gamepad、Audio、RGB、Time/RTC、Console、Server、Mirror 和 Runtime Monitor。应用不得直接访问 TFT、SPI、WiFi、Preferences 或硬件单例。
- AudioService 在麦克风页、显式耳返或 USB 录音期间按需采集 ES8311 ADC / GPIO6 的 8 kHz mono PCM16，在 PSRAM 保留最近 6 秒，并向页面、缓存回放和 USB WAV 工具提供有界接口；语音清晰度和长期采集仍待人工验收。
- Games 包含 Snake、Tetris、Breakout、Platformer、Blackjack、Minesweeper 和 2048。Platformer 的 1-1 至 8-4 数据由主机工具转换为固件内 C++ 资源，设备端不解析 CSV/XML/PNG。
- USB 控制台和 PGOS Studio 支持统一导航、状态查询、吞吐实验和 TCP 19002 屏幕镜像；当前镜像仍是完整 RGB565 帧，脏矩形/关键帧属于后续工作。
- shadow framebuffer 只在无线镜像连接或 USB 明确请求截图时更新；关闭镜像时不要把它重新放回高频渲染路径。
- System Tools 已集成中文大语言模型：当前 7.56M 参数、3.92MB INT4 模型从独立 Flash 分区 mmap，进入生成页后按需占用约 4.08 MiB PSRAM，页面提供预设开头、流式输出、A 重新生成和 B 返回列表；双轮真机短测约 5.1--5.2 tok/s，退出后 PSRAM 完整回收。

实现细节、资源预算和真机证据以项目路线和实验记录为准，不在本文件复制完整验收报告。

## 5. 开发纪律

- 一次只增加一个外设、应用或通信层；先编译，再主机测试，再上板，最后写实验记录。
- 异步网络、BLE、镜像和吞吐工作必须有边界、超时、取消和限流，不能阻塞屏幕、USB、输入或音频。
- 应用只通过 Service/AppContext 使用能力；服务回调入队，不直接调用页面或绘屏；不使用跨文件可写全局状态。
- 持久化数据必须有独立 namespace、schema/version 和合理的写入合并策略；游戏运行过程不要频繁写 Flash。
- 家庭 Wi-Fi 密码、token 和本地服务器配置只能放在未跟踪配置或设备 NVS，不能提交 Git，也不能通过 TCP 调试命令泄露。
- 不自动运行厂商目录中的 EXE、APK、烧录器或不明二进制；需要时先在实验记录中说明。
- 不把大型厂商二进制资料复制进 Git，也不修改 `docs/vendor` 原始资料。
- 真实硬件结论必须记录在对应实验文档中，不只保留在聊天记录里。
- 不用 `git reset --hard`、`git checkout --` 等命令覆盖用户改动；先检查工作区再编辑。

## 6. 当前待验收与后续

优先顺序如下；完成后更新对应实验记录和本节：

1. 大语言模型的实体交互和 P96/F512 双轮冒烟已通过；继续做固定提示词批量质量评价、Wi-Fi/BT 共存和长期稳定性观察，再决定采样优化或 P192 扩容。
2. RGB Breathe/Heartbeat 的 10ms 渐变、颜色顺序、白色满亮度和长期稳定性。
3. Platformer 的 Xbox 手感、碰撞、区域机制、城堡/Bowser、反馈和 32 关长测。
4. Breakout 三关、挡板与角落碰撞、Top 5、碎片、音效、震动和长期稳定性。
5. Blackjack 破产补助、牌靴重洗、实体手柄、震动/RGB 反馈和筹码持久化。
6. Minesweeper 修正版固件烧录后的入口、四档棋盘、首击安全、插旗/chord、暂停、成绩、连续移动、反馈和息屏唤醒。
7. 麦克风的人声清晰度、增益/削波、耳返反馈、连续采集和 WAV 长测。
8. PGOS Studio 的 keyframe + dirty rectangles、UDP/mDNS 发现、PCF8563 电池保持与 SNTP 回写。
9. 统一输入扩展到实体按键/编码器，再逐项探索 ADC、TF/扩展接口和 OTA。

## 7. 通用资产和验证入口

- `tools/playground_console.py`：USB 控制台、命令发送、日志协调。
- `tools/capture_screen.py`：RGB565 截图、PNG 和 Windows 剪贴板导出。
- `tools/capture_microphone.py`：USB 麦克风 PCM 帧校验和 mono WAV 导出。
- `tools/pgos_studio.py` / `tools/pgos_server.py`：无线 Studio 和协议诊断服务器。
- `tools/tests/`：主机规则、输入、资源转换、协议和截图测试。
- `docs/knowledge/`：显示、字体、USB、音频、I²C、输入、内存和测试经验。

常用验证命令：

```powershell
pio run -e playground
python -m unittest discover -s tools/tests -p "test_*.py"
```

主机测试通过不等于真机验收完成；烧录、截图、串口日志和硬件行为仍需按实验记录留证。
