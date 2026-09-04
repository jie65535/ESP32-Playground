# ESP32 Playground / PlaygroundOS

[English](README.md) | 中文

面向 QD 电子 ES3N28P（ESP32-S3 R8N16）开发板的个人硬件实验项目。
项目从显示、输入、USB、GPIO、RGB、音频、I2C、Wi-Fi、BLE 和局域网通信等
小实验逐步收敛为 PlaygroundOS（PGOS）：由系统服务、前台应用和统一输入协议
组成的轻量设备基座。

> 当前仓库是个人实验项目的开发快照，不是面向量产的产品固件。功能状态以代码和
> `docs/experiments/` 中的记录为准；真机验证结果不等同于对其它硬件的保证。

## 功能概览

| 模块 | 内容 |
| --- | --- |
| PGOS Shell | LVGL 9.5 桌面、分级菜单、状态栏、亮度/息屏、统一返回与 Home |
| 显示 | ILI9341V、SPI2 40 MHz、DMA 刷屏、RGB565 截图和无线镜像 |
| 输入 | 板载输入、USB CDC 控制台、Xbox BLE 手柄和有界事件队列 |
| 音频 | ES8311 播放、8 kHz 单声道麦克风采集、降噪、VAD 和 WAV 导出 |
| 连接 | 2.4 GHz Wi-Fi Station、SNTP/PCF8563 RTC、PGOS Studio TCP 通道 |
| 游戏 | Snake、Tetris、Breakout、Blackjack、Minesweeper、2048 和 Platformer |
| 本地 AI | 7.56M 参数中文 TinyLM；模型从独立 Flash 分区映射，按需使用 PSRAM |

## 硬件

| 功能 | 配置 |
| --- | --- |
| 主控 | ESP32-S3 R8N16，16 MB QIO Flash，8 MB OPI PSRAM |
| LCD | ILI9341V，固件横屏 320x240；CS 10、MOSI 11、SCK 12、MISO 13、DC 46、背光 45 |
| USB | 原生 USB CDC，GPIO19/20 |
| RGB | WS2812，GPIO42 |
| 音频 | ES8311；I2C GPIO15/16；I2S MCLK/BCLK/WS/DAC GPIO4/5/7/8；功放 EN GPIO1 |
| RTC | PCF8563，7-bit 地址 `0x51`，与 ES8311 共用 I2C 总线 |

接线、电气边界和保留脚见[硬件基线](docs/HARDWARE.md)。使用外接模块或扬声器前，
请确认 3.3 V 兼容、共地和电源安全。

## 运行画面

这些截图来自开发过程中的真机捕获，选择了能代表系统、游戏、本地模型和音频功能的画面。

| PGOS 桌面 | 2048 |
| --- | --- |
| ![PGOS 桌面](docs/images/pgos-home.png) | ![2048 游戏](docs/images/game-2048.png) |

| TinyLM 预设 | 麦克风工具 |
| --- | --- |
| ![TinyLM 预设](docs/images/tinylm-presets.png) | ![麦克风工具](docs/images/audio-microphone.png) |

| Breakout |
| --- |
| ![Breakout 游戏](docs/images/game-breakout.png) |

## 快速开始

### 环境

- [PlatformIO](https://platformio.org/) CLI
- Python 3.10 或更高版本
- 一块 ES3N28P R8N16 和可传输数据的 USB 线

### 构建、烧录和串口监视

```powershell
pio run -e playground
pio run -e playground -t upload --upload-port COMx
pio device monitor --port COMx --baud 115200
```

PlatformIO 会按 `platformio.ini` 和 `main/idf_component.yml` 恢复构建依赖。组件快照、
锁定提交和当前无法完全离线复现的部分见[依赖说明](docs/DEPENDENCIES.md)。

### 主机工具

```powershell
python -m pip install -r tools/requirements.txt
python tools/playground_console.py --list
python tools/playground_console.py --port COMx
python tools/capture_screen.py --port COMx --output captures/home.png
python tools/capture_microphone.py --port COMx --duration 3000 --output captures/mic-test.wav
```

USB 控制台支持导航、页面直达、状态查询、RGB565 截图和麦克风录音。Wi-Fi SSID、
密码和服务器地址通过设备页面或控制台在运行时配置，不能写入源码；示例头文件
`include/secrets.example.h` 只包含占位符。

### PGOS Studio

```powershell
python -m pip install -r tools/requirements-studio.txt
python tools/pgos_studio.py --listen 0.0.0.0 --port 19000
```

Studio 使用 TCP 19000 控制、19001 吞吐测试和 19002 屏幕镜像。当前控制链路没有
TLS 或认证，只应在可信的家庭局域网中临时使用，详见 [SECURITY.md](SECURITY.md)。

## 测试

```powershell
python -m unittest discover -s tools/tests -p "test_*.py"
pio run -e playground
```

主机测试覆盖游戏规则、输入策略、资源转换、协议工具和演示数据。通过测试不代表
已经完成屏幕、手柄、音频、无线共存或长期稳定性的真机验收；请按对应实验记录留证。

## 仓库结构

```text
main/       固件入口和 ESP-IDF 编排
src/        PGOS Apps、Services、游戏、UI 和固定第三方源码
include/    公共头文件和示例配置
boards/     PlatformIO 自定义板卡定义
tools/      控制台、Studio、资源转换和主机测试
docs/       硬件基线、架构、路线和实验记录
licenses/   字体与图标许可证文本
```

推荐阅读：

- [功能清单](docs/FEATURES.md)
- [项目路线](docs/PROJECT_PLAN.md)
- [PGOS 架构](docs/ARCHITECTURE.md)
- [Wi-Fi 与局域网方案](docs/WIFI_PLAN.md)
- [实验记录目录](docs/experiments/)
- [可复用经验知识库](docs/knowledge/README.md)
- [第三方依赖策略](docs/DEPENDENCIES.md)
- [第三方声明](THIRD_PARTY_NOTICES.md)

## 资源与发布边界

- 中文 TinyLM 的训练/导出源码位于 `src/third_party/esp32_ai/`，软件快照按其目录
  内的 MIT 许可证提供。约 430 MB 原始数据、训练中间文件、checkpoint 和 `model.bin`
  不在仓库中；数据集、词表和模型权利必须分别核对。
- `src/games/Platformer*` 包含从第三方 SMB 粉丝项目转换得到的关卡数据和像素资源，
  并使用 Mario/Nintendo 相关名称。它们不受根目录 MIT 自动授权；在重新分发源码、
  固件或二进制前，必须取得相应授权，或替换为拥有明确许可的原创资源。
- 字体、图标、VAD、SpeexDSP 以及构建时下载的组件各自适用原始许可证，完整范围见
  [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
- `docs/vendor/` 仅是开发板厂商参考资料说明，原始资料包默认不纳入 Git；不要把厂商
  EXE、APK、烧录器或大体积资料上传到公共仓库。

## 许可证

本项目原创代码和文档（除文件或目录另有声明者外）以 [MIT License](LICENSE) 发布。
MIT 不会覆盖第三方源码、字体、模型/数据、厂商资料或 SMB 派生资源；分发时请同时
保留相应的版权和许可证文本。项目作者：[@jie65535](https://github.com/jie65535)。
