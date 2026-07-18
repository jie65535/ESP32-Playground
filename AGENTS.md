# ESP32 Playground 新对话交接说明

> 最后更新：2026-07-18
> 工作目录：`G:\MCU\ESP32Playground`
> 当前阶段：屏幕与板卡信息展示

## 1. 项目定位

ESP32 Playground 是一个独立的个人实验项目，目标是探索 QD 电子 ES3N28P 无触摸版 ESP32-S3 开发板的硬件能力。允许在实验中使用 Wi-Fi、BLE、音频、麦克风、I²C、网络服务器、OTA 等能力，但每一步都要先建立最小可验证样例。

厂商资料包只是开发板参考资料，保存在 `docs/vendor`；原始文件不应被实验代码修改。

## 2. 必读顺序

开始新任务前按以下顺序阅读：

1. `README.md`
2. `docs/HARDWARE.md`
3. `docs/PROJECT_PLAN.md`
4. `docs/ARCHITECTURE.md`（涉及平台、应用或模块边界时）
5. `docs/WIFI_PLAN.md`（涉及网络时）
6. `docs/knowledge/README.md`（需要复用已有经验时）
7. 对应的 `docs/experiments/*.md`
8. 需要查厂商原文时，进入 `docs/vendor/2.8inch_IPS_ESP32-S3_ILI9341V_ES3C28P_ES3N28P_V1.0`

发生冲突时：

- `docs/HARDWARE.md` 是当前项目采用的板卡引脚基线。
- 厂商原始资料是器件和电路的事实来源。
- 实验记录中的“实测”只代表对应日期、硬件和 Git 提交，不自动升级为固定规格。
- 如果厂商示例注释与原理图或真机不一致，先记录差异，再以原理图和实测为准。

## 3. 当前硬件基线

- 开发板：QD 电子 ES3N28P，无触摸版。
- 主控：ESP32-S3 R8N16，16MB QIO Flash，8MB OPI PSRAM。
- 屏幕：ILI9341V，240×320 物理方向，固件使用 320×240 横屏。
- 原生 USB CDC：GPIO19/20，Arduino `Serial`。
- LCD：CS GPIO10，MOSI GPIO11，SCK GPIO12，MISO GPIO13，DC GPIO46，背光 GPIO45。
- WS2812 RGB：GPIO42。
- 音频：ES8311 共用 I²C GPIO15/16；I²S MCLK/BCLK/WS/DAC 为 GPIO4/5/7/8；功放使能 GPIO1，实际为低电平使能。
- 板载麦克风采集方向使用 GPIO6，只有完成单独实验并确认电路后才启用。
- BOOT 键 GPIO0 仅用于下载/启动，不作为普通业务按键。
- GPIO2、GPIO3、GPIO14、GPIO21、GPIO43、GPIO44 是可进一步实验的扩展脚，但使用前要核对启动、电平和外部连接。

## 4. 当前实现状态

- 已创建 PlatformIO `playground` 环境。
- 当前固件由 PlaygroundOS `SystemKernel` 编排，显示、控制台、Wi-Fi 和现有页面已拆分为独立 Service/App 模块。
- DisplayService 使用 320×240 RGB565 TFT_eSprite 离屏画布，优先分配到 PSRAM，完整绘制后一次推送；串口状态每 10 秒低频输出。
- WifiService 通过 USB 控制台扫描/选择 SSID、输入密码并保存到设备 NVS；连接采用非阻塞超时、扫描重试和退避重连，屏幕和 USB 显示状态、IP、RSSI 与重连次数。
- 当前交互仍以 USB 键盘控制台为主，不要求外接业务按键；暂不启动局域网服务和手机式设置向导。
- 真实硬件验证应记录在对应实验文档中，不要只在聊天里保留结论。

## 5. 开发纪律

- 每次只增加一个外设或一个通信层，先编译，再上板，再记录结果。
- 网络任务不得长时间阻塞屏幕、USB、按键或音频。
- 家庭 Wi-Fi 密码只能放在未跟踪的本地配置或 NVS，不能提交 Git。
- 不自动运行厂商目录中的 EXE、APK 或烧录工具；需要时先在实验文档中说明。
- 不把大型厂商二进制资料复制进 Git；`docs/vendor` 默认被 `.gitignore` 忽略。
- 外接电源、串口电平、扬声器和麦克风实验必须先确认电气边界，不能把不确定的外部电压直接接到 GPIO 或开发板 3.3V。

## 6. 下一步

1. 在当前样机上继续确认 PlaygroundOS 模块拆分后的显示、USB CDC 和 Wi-Fi Station 行为。
2. 增加 Launcher、Settings 和统一输入事件，再实现手机式 Wi-Fi 开关/扫描列表/文本输入。
3. 增加 NTP 状态显示，并保持网络任务非阻塞。
4. 设计局域网无线控制台：优先设备主动 TCP 长连接，另提供 HTTP/JSON 状态与命令接口。
5. 增加独立的吞吐/延迟测试，不把测试流量混入控制命令通道。
6. 再逐项探索 RGB、音频、麦克风、I²C/RTC、ADC、BLE、TF/扩展接口和 OTA。

## 7. 已迁移的通用资产

- Fusion Pixel Font / Fusion Bold Pixel Font 精简点阵字库、BDF 生成器、许可证和字体测试。
- `tools/playground_console.py`：USB CDC 键盘控制、命令发送、串口日志线程和截图暂停协调。
- `tools/capture_screen.py`：RGB565 原始画布转 PNG、Windows CF_DIB 剪贴板和固定帧协议。
- `docs/knowledge/`：颜色校准、离屏渲染、字体、USB、板型、音频、I²C、输入、内存和测试经验。

这些资产都是通用的显示、输入和调试能力；具体实验应在本项目中重新定义页面和数据模型。
