# Playground 项目路线

长期平台方向：PlaygroundOS（PGOS）。先建立稳定的服务、应用和连接边界，再逐项加入板载外设与实验应用。

## 已完成：屏幕与板卡信息

目标是建立可重复的最小固件：

- 点亮 ILI9341 横屏。
- 显示芯片、Flash、PSRAM、运行时间和空闲内存。
- 通过 USB CDC 输出启动信息和低频状态。
- 不连接外部模块，不依赖外部按键，不启用 Wi-Fi。
- 使用 PSRAM 离屏 Sprite，支持 USB 键盘控制和 RGB565 无损截图。

## 当前：Wi-Fi Station 与 PGOS Studio

PlaygroundOS 第一阶段基座已建立：`SystemKernel`、`AppManager`、`DisplayService`、`ConsoleService` 和 `WifiService` 已在 `src/` 下分层，System、Color Lab、Display Settings 和 Network 作为前台 App 运行。

2026-07-20 完成第一轮 Shell 架构回收：桌面收敛为 Games / Settings /
System Tools 三个父菜单，`AppManager` 保存父页面历史，Back 逐级返回而 Home
清空历史回桌面；菜单复用同一个数据驱动 `MenuApp`。Shell 正文和卡片已汉化，
大字号标题与游戏 HUD 保留英文，并引入独立 Lucide 图标资源。

- 连接家庭 2.4 GHz 路由器。
- 显示 SSID、IP、RSSI 和重连次数。
- USB 命令支持 `wifi status`、`wifi scan`、`wifi reconnect`。
- 使用非阻塞连接状态机和退避重连。
- Network 页面可在设备端扫描、选择 SSID，并通过方向键软键盘输入密码；开放网络直接保存。USB 控制台继续作为隐藏输入和维护后备，密码只写入设备 NVS，不进入 Git。
- 支持隐藏 SSID 的手工输入和开放网络。
- NTP 状态显示留待入网稳定后单独增加。
- 目标服务器地址保存在 NVS；`server set <host> <port>` 后自动启用控制连接，Wi-Fi 或上位机恢复时退避重连。
- PGOS Studio 直接监听控制 TCP，提供方向/确认/返回/Home、结构化状态与有界日志，不耦合具体设备页面。
- 独立 TCP 19002 已实现约 5 FPS 的 320×240 RGB565 屏幕镜像 MVP；镜像由上位机临时控制，不写入设备设置。

显示设置基座已加入：GPIO45 使用 PWM 控制背光亮度，空闲计时只关闭背光而不停止系统服务；亮度和息屏时间保存到独立 NVS namespace，并由 Display App 修改。

声音基座已加入最小实验：ES8311/I²S 播放固定 1 kHz 反馈音，声音页支持音量、交互反馈音开关和试听；默认音量 60%、反馈音关闭，初始化失败时页面仍可显示并报告 unavailable。

麦克风基座已接入 ES8311 ADC / GPIO6：音频任务在麦克风页、显式耳返或 USB 录音期间按需采集 8 kHz mono PCM16，计算 RMS/Peak/Level，并在 PSRAM 保留最近 6 秒；麦克风页提供三档输入增益、持久化且默认开启的 SpeexDSP 固定点频域降噪、WebRTC VAD 驱动的人声 AGC/限幅、默认关闭的衰减耳返和最近缓存回放。Speex 工作区在启动阶段一次性放入 PSRAM，本机播放期间暂停 DSP 并丢弃采集，退出页面后停止 RX/DSP。USB `mic record` 使用带 request/sequence/CRC32 的二进制帧导出，`tools/capture_microphone.py` 自动管理临时采集并保存 WAV。COM3 已验证 `normal` 增益、低音量背景音乐降噪、VAD 人声增益、8 次页面进出稳定性及录音协议；最终人声主观试听和更长时间稳定性仍待验收。

RGB 基座已加入最小实验：`RgbService` 通过 GPIO42/RMT 驱动板载 WS2812；Power 默认关闭且不持久化，灯效、基色、亮度和速度保存到独立 NVS namespace。RGB Light App 的静态、呼吸、彩虹、心跳和闪烁均由非阻塞状态机运行，离开页面后仍可继续作为后台灯光服务。

PCF8563 RTC 基座已加入最小实现：`I2cBusService` 统一拥有 GPIO15/16 共享总线，`TimeService` 以 `0x51` 探测并读取电池时钟，Time 页面显示本地时间，USB 支持 `time set`；Wi-Fi 连接后由 SNTP 自动校对并回写 RTC。模块缺失、VL、STOP 或非法字段时会降级；NTP 回退、运行中恢复、冷启动 RTC 读取和 RTC 回写已真机通过，电池保持和长期漂移仍待焊接后记录。

## 当前：小游戏、固定步进与回合制事件动画

- Snake、Tetris、Breakout、Blackjack、Minesweeper 和 2048 均作为独立前台 App 使用 LVGL 单一自绘面，不直接访问显示硬件，也不创建游戏元素子对象。
- Platformer 已从 1-1 扩展到 1-1 至 8-4 共 32 关：主机端严格转换 192 个 CSV 图层和 32 份属性文件，固件端使用按行 RLE、调色板索引图块、稀疏修改表及固定敌人/平台/投射物池。战役已接入跨区域/跨关水管、8-4 循环点、水下、藤蔓奖励区、弹簧、动态平台/滑轮、火焰棒、旗杆、Bowser 桥战、敌人差异化状态机和 NVS 继续进度；App 仍只拥有一个 `RenderSurface` 和一块按需 PSRAM framebuffer。USB `platformer maptest [world-stage]` 可直达任意关，`platformer mapnext` 再逐段巡检。
- Xbox 手柄连续状态由游戏按帧采样，A/B/Home 和 D-pad 事件仍经过统一输入路由；USB/TCP 保留低频语义命令用于调试。
- 前三款游戏通过同一个可配置 `GameScoreService` 保存 Top 5，继续沿用各自独立
  NVS namespace 和 schema；动态过程不写 Flash。
- Breakout 首次引入 8ms 固定步进连续碰撞和定长碎片池，主机测试与固件构建已通过，真机操控、碰撞边界、DMA 刷新、音效和震动仍待验证。
- Blackjack 使用纯 C++ `BlackjackEngine` 产生洗牌、发牌、翻牌和结算事件，
  `BlackjackApp` 顺序消费事件并锁定牌桌输入；筹码与胜负统计由独立
  `BlackjackProfileService` 保存，不把资金模型硬塞进 Top 5 抽象。
- Minesweeper 使用纯 C++ `MinesweeperEngine` 和逻辑推导求解器生成首击安全且无猜测可解的候选棋盘；App 每 tick 只验证一个候选，提供 18/11/10px 分级棋盘、插旗/chord、暂停、结果动画、音频/震动/RGB 反馈。`MinesweeperProfileService` 以独立 `pgos_mines` schema 保存三档 Top 5、局数、胜率和连胜；标准难度与自定义参数不混用记录。
- 2048 使用纯 C++ `Game2048Engine` 维护 4×4 方块、单次合并和 2/4 生成概率；`Game2048App` 以固定 motion 记录绘制 145ms 滑动、合并弹性和新块弹出，独立 `Game2048ProfileService` 保存 32 位最佳分/最高方块/统计，并接入音频、震动和 RGB 目标反馈。

## 当前：中文 TinyLM 本地生成

- `src/third_party/esp32_ai/` 内置训练、导出和设备推理源码；TinyStories-Zh-1M 原始 Parquet、训练 bin/checkpoint 和 `model.bin` 保持本地忽略，生成后的 8192-token `vocab.h` 与精简字体进入固件资产。
- 当前模型为 `V=8192, D=128, L=6, H=4, F=512, P=96, S=512, G=128`，总参数量 7,563,744；导出 INT4 模型 3,917,660 bytes，运行时模型主体从 Flash mmap，输出头与 KV/scratch 按需占用约 4.08 MiB PSRAM。
- `TinyLmService` 用有界任务/Token 队列隔离推理和 LVGL，退出应用后释放大块 PSRAM；`TinyLmApp` 提供四个预设开头、UTF-8 增量解码、数据集字符覆盖字体、A 重新生成和 B 返回列表。
- 16MB Flash 已迁移为两个 4MB OTA 槽、4MB `model` 分区、3.875MB SPIFFS 和保留 NVS/coredump；当前固件约 2.77MB，仍保留双 OTA 余量。
- 构建、64 项主机回归和 C/PyTorch golden 数值对比已通过；P96/F512 双轮真机冒烟确认约 5.1--5.2 tok/s、推理中仍空闲约 3.50MiB PSRAM 且退出后完整回收。2026-07-26 用户实体交互验收通过；固定提示词批量评价、Wi-Fi/BT 共存和长期稳定性继续观察。
- 若暂时取消 OTA，保留 4MiB App 和 SPIFFS 时可将模型分区扩到 8MiB；参数上限、P192/F512 后续候选、训练量、质量预期和非故事应用已记录在 `docs/experiments/029_tinylm_scaling_and_applications_plan.md`。当前只完成不改分区的 P96/F512 升级，尚未执行 8MiB 扩容。

## 随后：PGOS Studio 媒体与工具能力

- UDP 19003 / mDNS 服务发现：设备发现 Studio、Studio beacon 唤醒设备重连，并以持久化 `server_id` 防止误连。
- 镜像 FPS、应用层吞吐和控制 RTT 已加入；丢帧统计待增加。
- 脏矩形或轻量压缩、截图和录像。
- 电脑画面到设备 `RenderSurface` 的受控反向推送。
- 音频上/下行实验与设备专属调试面板。
- 保持 GUI 只依赖通用输入与能力协商，不为每个设备页面增加快捷入口。

## 当前：基础系统监视器

- 设备端 System 页面已升级为可滚动监视器，显示主循环占用、Heap/最低水位、PSRAM、固件/OTA Flash 和任务数。
- 当前 CPU 字段明确表示主循环 duty，不冒充双核总 CPU；完整 FreeRTOS idle/runtime 统计需要切换可配置的 ESP-IDF/Arduino 构建后再加入。
- 主循环末尾增加 1 ms 调度让步，让 idle/Wi-Fi 系统任务获得稳定运行窗口。
- 文件系统占用、温度（若硬件支持）和 Wi-Fi/控制通道历史曲线继续后补。
- Studio 复用同一份结构化快照，提供历史曲线；监视器本身不能阻塞镜像、音频和控制输入。
- LCD 异步 DMA flush 进入真机实验：用内部双缓冲把约 33 ms SPI 线速时间与 LVGL
  绘制重叠，失败时自动回退同步刷新。
- shadow framebuffer 当前只为完整帧镜像和截图保留；后续迁移到 keyframe + dirty
  rectangles，并用 LVGL snapshot 处理低频按需截图。

## 后续：局域网 HTTP/JSON

- ESP32 提供 `/api/v1/status` 和 `/api/v1/telemetry`。
- ESP32 周期性向电脑服务器发送 heartbeat。
- Python 服务器显示设备在线、IP、RSSI、内存和最近事件。
- 命令接口使用白名单和局域网 token。

## 其它实验

按一次只探索一个外设或协议：

- 实体按键
- ES8311 播放
- 麦克风语音清晰度、增益和长时间采集
- I²C 扫描、PCF8563 真机保持与长期漂移
- ADC 与功耗
- BLE
- TF/扩展接口（以原理图和实物为准）
- WebSocket、MQTT、OTA

MQTT 和 OTA 不作为 Wi-Fi 第一版的前置条件。
