# PlaygroundOS（PGOS）微型应用平台架构

PlaygroundOS（简称 PGOS）是 ESP32 Playground 的可扩展设备基座名称。它不是 Linux 替代品，而是运行在 Arduino-ESP32/FreeRTOS 之上的应用、服务和多连接交互层。

## 定位

Playground 可以演进为一台带屏幕、键盘、Wi-Fi、BLE 和音频能力的“按键式智能终端”。它提供桌面、设置、应用和系统服务，但不尝试复刻 Linux/Android 的进程、动态安装包、复杂权限和通用文件系统。

核心约束：同一时刻只运行一个前台应用；Wi-Fi、BLE、存储、控制台等由系统服务维护；应用通过稳定接口使用能力，不直接访问硬件全局对象。

## 分层

```text
main.cpp
   │
SystemKernel / AppManager
   ├── Launcher（桌面）
   ├── Settings（设置）
   ├── Apps（信息、网络、游戏、实验）
   └── Services
       ├── Display / Input
       ├── Storage / SettingsStore
       ├── WiFi / BLE
       ├── Console / NetworkTransport
       └── Audio / Time
```

依赖只能向下：应用不能互相包含，UI 不直接调用 `WiFi`/`Preferences`，网络回调不能直接绘屏，服务也不依赖具体应用。

## 应用模型

所有前台应用实现同一个最小生命周期：

```cpp
class IApp {
public:
    virtual AppId id() const = 0;
    virtual const char* name() const = 0;
    virtual void onEnter(AppContext& context) = 0;
    virtual void onExit(AppContext& context) = 0;
    virtual void onEvent(const AppEvent& event, AppContext& context) = 0;
    virtual void onTick(uint32_t nowMs, AppContext& context) = 0;
    virtual void onRender(UiCanvas& canvas) = 0;
};
```

应用由编译期 `AppRegistry` 注册，不做运行时下载和动态链接。新增应用只需要创建一个目录、实现接口并加入注册表。

建议首批应用：

- `LauncherApp`：桌面/应用列表和状态栏。
- `SettingsApp`：Wi-Fi、BLE、服务器、设备名和系统选项。
- `SystemInfoApp`：当前板卡、内存和运行时间页面。
- `DisplayTestApp`：颜色、字体和截图实验。
- `NetworkConsoleApp`：服务器发现、连接状态和吞吐测试。
- `MazeApp`、`PacmanApp`：独立小游戏，不直接操作系统服务。

无触摸屏更适合按键机式列表、分页或轮播桌面，不必复制手机图标网格。

## 系统服务

服务拥有硬件和协议状态，向应用暴露小接口与只读快照：

- `DisplayService`：TFT、PSRAM framebuffer、亮度和截图。
- `InputService`：USB 键盘、未来实体键和网络输入，统一转换为 Up/Down/Left/Right/Confirm/Back/Text。
- `SettingsStore`：NVS schema、版本迁移和写入频率控制。
- `WifiService`：开关、扫描、凭据、连接、重连、IP/RSSI。
- `BleService`：开关、扫描、配对和连接；未启用时不占用应用逻辑。
- `ConsoleService`：命令注册、帮助和 USB/无线传输适配。
- `ServerService`：UDP/mDNS 发现、手工地址、TCP 会话和心跳。
- `TimeService`：单调时钟、NTP 和时区。
- `AudioService`：ES8311 与 I²S；需要实时性时可拥有独立任务。

Wi-Fi、BLE 和服务器地址都作为设置项保存。服务器设置支持“自动发现列表”和“手动输入主机/IP + 端口”，与手机网络设置的交互一致。

## 事件与渲染

输入、Wi-Fi、BLE、计时器和服务器变化统一转换为 `AppEvent`。系统每轮执行：

```text
采集输入/服务事件 → AppManager 路由 → 当前 App 更新 → 完整离屏渲染 → 一次推屏
```

事件使用小型枚举/结构体和有界队列，避免跨模块共享可变全局状态。服务回调只入队，不直接调用应用和 UI。

## 连接与交互抽象

USB CDC、Wi-Fi TCP/WebSocket、BLE GATT 和未来实体按键使用同一条交互链路：

```text
TransportAdapter → SessionManager → ControlProtocol → InputRouter → AppManager
                                  ↘ StatePublisher → 远程控制台/手机 App
```

### TransportAdapter

传输适配器只负责收发字节或消息、分片重组和链路状态，不理解具体应用命令：

- `UsbCdcTransport`
- `TcpTransport`
- `BleGattTransport`
- `LocalKeyTransport`

新增一种连接方式只实现适配器，不修改应用。UDP/mDNS/BLE Advertising 属于发现层，不直接承载控制会话。

### SessionManager

会话层统一处理设备 ID、协议版本、握手、认证、心跳、重连、超时、限流和控制权。USB 可视为本地可信会话；Wi-Fi 使用 token/TLS 策略；BLE 使用配对/绑定。多客户端同时在线时区分 `controller` 与 `observer`，默认只有一个会话持有输入焦点。

### ControlProtocol

协议使用带版本和长度边界的帧，避免依赖串口换行。第一版可以传 JSON，吞吐和 BLE 分片需求明确后再换 CBOR 或紧凑二进制编码。重要命令带请求 ID 和结果响应，重复发送必须尽量保持幂等。

协议消息分为：

- `InputEvent`：方向、确认、返回、Home、按下/释放。
- `TextInput`：UTF-8 文本和编辑动作，用于密码、服务器地址等输入框。
- `SystemCommand`：截图、状态、重启网络、启动应用等白名单命令。
- `StateUpdate`：当前应用、菜单、网络状态、通知和遥测。
- `BulkData`：截图、日志和性能测试数据，使用独立通道或明确流控。

不把 PC/手机的原始键码直接传给应用。普通界面使用 `Up/Down/Confirm/Back` 等语义事件；需要同时检测多个按键的游戏可以订阅规范化的 `KeyDown/KeyUp`。

### InputRouter 与 StatePublisher

`InputRouter` 把本地键、USB 控制台、Wi-Fi 和 BLE 输入转换为同一种 `AppEvent`，再按优先级和焦点交给系统快捷键、桌面或前台应用。所有来源都带 `source/session` 元数据，便于权限、去重和调试。

`StatePublisher` 反向发布结构化状态。手机 App 通常渲染自己的原生界面，而不是持续接收 320×240 framebuffer；截图/画面流只作为调试、镜像或特定游戏模式。这样 BLE 低带宽链路也能高效操作。

## FreeRTOS 使用策略

Arduino-ESP32 已运行在 FreeRTOS 上，但第一阶段不为每个模块创建任务：

- `SystemKernel`、桌面和前台应用继续在协作式 `loopTask` 中运行。
- Wi-Fi/BLE 事件回调只投递消息。
- 只有音频流、持续 TCP 大吞吐或其它确实会阻塞的工作才建立专用任务。
- 专用任务通过队列/事件组交付结果，不能直接绘屏或修改应用状态。

这样保留简单、可调试的主循环，同时为后续实时任务留下边界。

## 推荐目录

```text
src/
  main.cpp
  core/          # SystemKernel、AppManager、事件、注册表
  services/      # Display/Input/Storage/WiFi/BLE/Console/Server/Audio
  shell/         # Launcher、Settings、状态栏和通用控件
  apps/
    system_info/
    display_test/
    network_console/
    maze/
    pacman/
```

每个应用目录只包含该应用的状态、渲染和测试资产。应用之间不直接 include；共享能力必须进入 `core`、`services` 或通用 UI 控件。

## 适合 Vibe Coding 的规则

1. `main.cpp` 只创建 `SystemKernel` 并转发 `setup/loop`。
2. 每次只新增一个服务或一个应用，不同时改动多个层。
3. 新应用从模板生成，目录内包含简短 README、资源预算和验证方法。
4. 接口优先写在小头文件中；实现可以替换，但应用不能依赖实现细节。
5. 不使用跨文件可写全局变量，不允许应用直接访问 `Serial`、`WiFi`、`Preferences` 或 TFT 单例。
6. 为 framebuffer、网络缓冲、音频缓冲和应用资源记录 RAM/PSRAM/Flash 预算。
7. 所有异步工作必须有超时、取消和状态快照；所有持久化项必须有 schema/version。
8. 完成编译、主机测试和真机记录后，功能才从实验升级为平台能力。

## 迁移顺序

1. 建立 `SystemKernel`、事件模型和服务接口，把现有显示、控制台、截图、Wi-Fi 从 `main.cpp` 迁出。
2. 把当前三个页面迁移为 `SystemInfoApp`、`DisplayTestApp` 和 `SettingsApp/WiFi`。
3. 增加按键式 `LauncherApp`，一次只激活一个前台应用。
4. 完成 Wi-Fi 开关、扫描列表、密码输入、服务器发现和手工地址设置。
5. 加入网络控制台与独立吞吐测试。
6. 再增加 BLE、小游戏、音频等应用；只有出现真实阻塞需求时才增加 FreeRTOS 专用任务。

## 架构冻结前检查清单

以下项目应在继续增加应用前定下契约；具体实现可以分阶段完成。

### 现在必须确定

1. **设备身份**：稳定的 `device_id`、可编辑设备名、固件版本和能力列表。
2. **协议信封**：版本、消息类型、请求 ID、序号、长度、错误码和最大帧大小。
3. **输入语义**：导航、确认、返回、文本编辑、按下/释放、系统快捷键和事件来源。
4. **会话权**：controller/observer、单控制者、控制权超时、断线释放和重复命令规则。
5. **服务所有权**：每个硬件外设只有一个 Service 写入；应用通过接口读取快照或提交请求。
6. **持久化分区**：系统设置、网络凭据、服务器配置、应用数据和日志分别放置，带 schema/version 和迁移策略。
7. **资源预算**：前台 App framebuffer、Wi-Fi/BLE 缓冲、音频缓冲、游戏资源的 RAM/PSRAM/Flash 上限。
8. **失败恢复**：看门狗、连接超时、应用退出、异常重启计数、safe mode 和 factory reset。
9. **日志与诊断**：统一时间戳、级别、模块名、序号、环形缓存和 USB/无线输出策略。
10. **测试替身**：FakeTransport、FakeInput、FakeStorage 和 FakeClock，使协议与 App 能在主机测试。

### 可以延后实现

- BLE GATT 的具体 characteristic 布局。
- TCP 还是 WebSocket 的最终选择。
- TLS 硬件加速和证书更新。
- 手机端渲染全部页面，还是只做控制/状态面板。
- 应用资源压缩、主题系统、多语言和远程安装。
- 音频专用任务、低功耗睡眠和复杂电源策略。

### 容易导致返工的反模式

- 让每个 App 直接持有 `WiFi`、`Serial` 或 TFT 单例。
- 把 USB 换行命令直接当成长期无线协议。
- 把 PC/手机原始键码直接暴露给应用。
- 用 framebuffer 作为手机 App 的唯一 UI 协议。
- 把密码、token、服务器地址散落在多个 Preferences namespace。
- 没有最大帧长、队列上限、超时和取消路径。
- 为每个小模块创建 FreeRTOS 任务，最后无法判断谁拥有状态。

当前 16 MB Flash 的 `default_16MB.csv` 已包含双 OTA app 分区、NVS、SPIFFS 和 coredump；后续可继续沿用，但应用资源和 OTA 包大小必须纳入 Flash 预算，不能等功能堆满后再调整分区。
