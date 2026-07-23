# ESP32 Playground 新对话交接说明

> 最后更新：2026-07-23
> 工作目录：`G:\MCU\ESP32Playground`
> 当前阶段：PGOS 应用基座 / LCD DMA / PGOS Studio / 板载 RGB / PCF8563 RTC / 扫雷待真机验收

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
- DisplayService 默认使用 ESP-IDF 原生 `esp_lcd_ili9341` 2.0.2、SPI2/FSPI 40MHz 和 DMA 完成回调；LVGL 9.5 使用两块 320×40 内部 DMA-capable RGB565 缓冲。硬件已实测原生 DMA 首帧、页面转场和 Xbox 手柄交互正常；CPU 配置已恢复到 240MHz，交互转场明显比 160MHz 流畅。旧 TFT_eSPI 已从默认构建移除；BitmapFont 保留为独立 RGB565 画布资产。
- 320×240 PSRAM shadow framebuffer 只在无线镜像已连接或 USB 截图明确请求时更新；镜像关闭时 copy 热路径约为 0ms，新镜像连接先强制完整 keyframe。
- `UiRuntime` 统一管理 LVGL 显示驱动、主题、22px 状态栏、右上角有效时间源 `HH:MM`、蓝牙状态、页面转场和焦点卡片；桌面已收敛为 Games / Settings / System Tools 三级菜单，四个菜单实例复用数据驱动 `MenuApp`，Back 通过 `AppManager` 父页面历史逐级返回，Home 清空历史回桌面。Shell 卡片、正文和设置值已使用 Fusion Pixel 精简字库汉化，24px 标题与游戏 HUD 保留英文；17 个 Lucide 24×24 A8 图标通过独立 `UiIcon` 资源使用，不再让卡片绑定 LVGL 内置符号。
- DisplayService 使用 GPIO45 LEDC PWM 控制背光，提供亮度、空闲息屏、活动唤醒和 `pgos_display` NVS 持久化；默认 100% / Never，第一次导航输入只唤醒屏幕而不误操作页面。
- 页面根节点是纵向滚动视口，焦点项使用非线性滚动自动靠近视口中心；通用卡片为 44px 高、步距 52px、固定 2px 边框，避免焦点态内容区移动。无页级副标题时首卡 y=50，需要说明时 y=64；卡片副标题只在状态、限制或特殊逻辑无法由标题表达时创建。单行标题按卡片中线对齐，两行卡片使用 y=2/21 围绕中线排布。状态栏使用波纹 Wi-Fi 图标和服务连接图标，不把诊断数值挤进顶栏。
- AudioService 使用已验证的 ES8311/I²S 方向，提供音量、反馈音、试听和 `pgos_audio` NVS 持久化；音量采用小扬声器校准的分段 dB 曲线并在 0 dB 封顶，Sound 页面在音频硬件不可用时降级显示。
- `I2cBusService` 统一拥有 GPIO15/16 的共享 `Wire` 总线；`TimeService` 以 `0x51` 读取 PCF8563，Time 页面显示完整日期时间，Wi-Fi 连接后由 SNTP（UTC+8）自动校时并回写 RTC，USB 保留 `time set` 手动设置；模块缺失、VL、STOP 或非法字段时降级。
- RgbService 使用 Arduino-ESP32 RMT 在 GPIO42 驱动板载 WS2812，提供 Solid/Breathe/Rainbow/Heartbeat/Sparkle；呼吸与心跳使用 10ms smoothstep 渐变。Power 不持久化，Effect/Palette/Brightness/Speed 保存到 `pgos_rgb`，750ms 合并写入。重启实测恢复 `Breathe / Violet / 50% / Fast`，同时保持 `state=off`。
- WifiService 通过 USB 控制台扫描/选择 SSID、输入密码并保存到设备 NVS；连接采用非阻塞超时、扫描重试和退避重连，屏幕和 USB 显示状态、IP、RSSI 与重连次数。
- BleGamepadService 使用 Bluepad32 4.2.0 接入 Xbox BLE HID（已实测 `XBox One` VID/PID `045E:0B13`）。启动/用户请求提供 60 秒配对扫描，连接后停扫；意外掉线立即进行 10 秒重连扫描，主动或空闲断开先等待 30 秒，再以 10 秒扫描 / 20 秒暂停的窗口自动重连。空闲活动以连接首帧的摇杆/扳机中心值为基线并使用迟滞状态，避免带偏移回中后锁存活动；Controller 页面和 `gamepad status` 显示扫描阶段、重连倒计时、输入年龄及模拟量诊断值。默认 15 分钟断开设置保存到 `pgos_gamepad`。
- USB CDC 与 TCP 命令已统一进入有界 InputRouter；ServerService 支持主动 TCP HELLO/heartbeat/PING-PONG、白名单远程导航和带请求号 ACK/STATE。
- Snake、Tetris、Breakout、Blackjack 和 Minesweeper 已作为独立前台小游戏接入 Games 菜单，均使用单一 LVGL 自绘面。前三款通过同一个可配置 `GameScoreService` 读写各自原有的 Top 5 NVS namespace/schema；Blackjack 使用纯 C++ `BlackjackEngine`、事件动画队列和独立 `BlackjackProfileService`，支持要牌、停牌、双倍、自然牌 3:2、1000 初始筹码及低于最低下注时恢复到 500 的 PGOS 补助。Minesweeper 使用纯 C++ 规则引擎/集合包含求解器，初级 9×9/10、中级 16×16/40、专家 30×16/99 及 8–30×8–16 自定义棋盘均首击九宫格安全且默认无猜测；App 提供插旗/chord、暂停、18/11/10px 分级绘制、波纹/旗帜/胜负演出、音效/震动/RGB 反馈及 `pgos_mines` 独立成绩资料。Breakout 提供三关、三条命、双耐久砖、8ms 固定步进碰撞、可控反弹角与定长碎片池；COM3 已验证标题、运行、击砖计分、暂停输入锁定和返回桌面，完整三关与 Xbox 手感仍待人工复测。
- Platformer 已从 1-1 扩展为 1-1 至 8-4 共 32 关战役：主机工具把参考工程的 192 个 CSV 图层、32 份属性和三张图集转换为按行 RLE 与调色板索引 C++ 数据，设备端不解析外部文件。纯 C++ `PlatformerEngine` 已接入区域/关卡水管、8-4 循环、水下、藤蔓、弹簧、移动平台/滑轮、火焰棒、旗杆、Bowser 桥战、主要敌人差异化状态机及 NVS 继续进度；标题页默认选中继续点，并按参考 `MenuSystem` 提供全部 32 关的 WORLD/STAGE 手柄选关与首屏预览。运行时使用固定实体池、PSRAM 解码图块缓存和一块按需 framebuffer。运行画面绕过 LVGL 自绘热路径，以 6 个 320×40 区块直接提交 LCD DMA，真机约 38.5ms/frame；单次 320×218 PSRAM 提交因内部 DMA bounce buffer 分配失败而禁用。玩家地图碰撞已按参考工程的方向 roundness、底部一格蹲姿碰撞和脚底锚定实现；龟壳计分/高度、敌人转向、隐藏砖、花、管道/旗杆层级及伤害无敌等细节已复核。40 项 Python 测试入口与固件构建通过，实体 Xbox 的完整 32 关长测仍待复测。
- PGOS Studio 已提供无线四方向/确认/返回/Home、运行状态、RTT、上下行吞吐测试和 TCP 19002 屏幕镜像；当前镜像仍发送完整 RGB565 帧，下一步是 keyframe + dirty rectangles。
- System 页面已提供主循环 duty、Heap/最低水位、PSRAM、Flash/OTA、任务数和各阶段耗时；这里的 CPU 指标是主循环 duty，不冒充双核总 CPU。
- 当前分支 `dev`，基线提交为 `cc4cac5`；工作区保留本轮待提交的 Platformer 32 关战役和此前共享 `CanvasDraw`/maptest 改动。最终烧录构建 RAM 87,544 / 327,680 bytes，Flash 2,354,777 / 6,553,600 bytes，Python 40 项测试入口通过；32 个首屏 contact sheet 已检查，COM3 烧录及标题选关、1-1、2-1、1-2 地下、8-4 城堡/水下和最新 1-2 运行截图通过，结果见 `docs/experiments/021_platformer_campaign.md`。Blackjack 的 Games 入口、标题、下注、发牌中、玩家回合和结算状态此前已完成 320×240 截图检查：中文与四种花色无缺字，PG 牌背、牌靴、底部操作栏和筹码结果正常；按钮与结算栏文字按字体行高垂直居中，像素中心误差为 0-0.5px。真机跑通自然 Blackjack 3:2、普通胜负和双倍下注。破产补助触发页、实体 Xbox 震动手感和 RGB 开启后的瞬时反馈仍待人工复测；Breakout 完整三关与 Xbox 手感、以及带偏移回中后的完整 5 分钟空闲断开和重连窗口仍待真机复测。
- 本轮扫雷已完成主机交付门槛：`MinesweeperEngine`、记录、布局/演出、控制台和图标测试与仓库全量入口共 45 项通过，当前修正版固件构建 RAM 107,944 / 327,680 bytes，Flash 2,393,657 / 6,553,600 bytes；离线九态 contact sheet 为 `captures/minesweeper/contact_sheet.png`。2026-07-23 首次真机验收发现标题副标题的 `×`/`·` 缺字、单次 D-pad 被重复采样、“同盘”语义不合理以及专家盘光标/移动手感不足；当前工作区已改为 ASCII 副标题、环绕安全输入去重、“再来一局/难度”双操作、完整高对比光标和独立 X/Y 八方向加速导航，修正版尚未烧录。准确范围、验证命令和待验收清单见 `docs/experiments/023_minesweeper_game.md`。
- 曾尝试“原生 FSPI + 80MHz”组合，真机出现花屏且实体屏停止刷新；当前原生 `esp_lcd` 固定 SPI2/40MHz，不再把 80MHz 作为默认配置。后续若重测必须一次只改变一个变量。
- 真实硬件验证应记录在对应实验文档中，不要只在聊天里保留结论。

## 5. 开发纪律

- 每次只增加一个外设或一个通信层，先编译，再上板，再记录结果。
- 网络任务不得长时间阻塞屏幕、USB、按键或音频。
- 家庭 Wi-Fi 密码只能放在未跟踪的本地配置或 NVS，不能提交 Git。
- 不自动运行厂商目录中的 EXE、APK 或烧录工具；需要时先在实验文档中说明。
- 不把大型厂商二进制资料复制进 Git；`docs/vendor` 默认被 `.gitignore` 忽略。
- 外接电源、串口电平、扬声器和麦克风实验必须先确认电气边界，不能把不确定的外部电压直接接到 GPIO 或开发板 3.3V。

## 6. 下一步

1. 让用户复测 `3073e00` 之后 Breathe/Heartbeat 的 10ms smoothstep 渐变是否足够丝滑，并确认七种颜色顺序、100% 白色与长期稳定性。
1a. 用户使用 Xbox 从当前 1-2 继续复测 Platformer，先确认大马下蹲受伤时脚底不跳格、单格坑和两格高通道可进入、龟壳脚底/转向/计分正常，再进行区域水管、藤蔓/平台/滑轮、城堡循环、Bowser、声音震动和 32 关长局稳定性测试。
2. 烧录并复测 Breakout 的三关流程、挡板手感、角落碰撞、Top 5、碎片、音效、震动和长期稳定性。
3. 复测 Blackjack 的破产补助、长局牌靴重洗、实体 Xbox 操作/震动、RGB 开启后的瞬时反馈和长期筹码持久化。
4. 烧录并集中验收 Minesweeper：Games 入口、四档棋盘、首击边角安全、插旗/chord、暂停排除计时、再来一局/难度、Top 5/统计持久化、Xbox 连续移动、结算锁定、音效/震动/RGB 与息屏首次输入。
5. 把镜像协议从固定 153600-byte 完整帧升级为首次 keyframe + dirty rectangles，由 Studio 在主机端合成画面。
6. 增加 UDP/mDNS 服务发现，减少手工配置 Studio 地址，同时保留持久化服务器身份和安全边界。
7. 复测 PCF8563 电池保持、断网计时和 Wi-Fi SNTP 自动回写，并记录 `time status` 的 `ntp=...` 状态。
8. 把统一输入语义扩展到实体按键/编码器，并增加受控的文本输入页面。
9. 为游戏和高频实验建立独立 RenderSurface，避免破坏系统 Shell 的 LVGL 所有权。
10. 再逐项探索麦克风、I²C/RTC、ADC、BLE、TF/扩展接口和 OTA。

## 7. 已迁移的通用资产

- Fusion Pixel Font / Fusion Bold Pixel Font 精简点阵字库、BDF 生成器、源码字符串自动补字脚本、许可证和字体测试。
- `tools/playground_console.py`：USB CDC 键盘控制、命令发送、串口日志线程和截图暂停协调。
- `tools/capture_screen.py`：RGB565 原始画布转 PNG、Windows CF_DIB 剪贴板和固定帧协议。
- `docs/knowledge/`：颜色校准、离屏渲染、字体、USB、板型、音频、I²C、输入、内存和测试经验。

这些资产都是通用的显示、输入和调试能力；具体实验应在本项目中重新定义页面和数据模型。
