# ESP32 Playground

面向 QD 电子 ES3N28P（ESP32-S3 R8N16）开发板的个人硬件实验项目。

项目以小步、可复现的实验方式探索显示、输入、USB、RGB、音频、I²C、Wi-Fi、BLE 和局域网通信。随着实验积累，固件逐步收敛为 PlaygroundOS（PGOS）：由系统服务、前台应用和统一输入/连接协议组成的轻量设备基座。

## 当前固件

- 使用 LVGL 9.5 和原生 LCD DMA 驱动，提供按键式桌面、设置页、系统工具、亮度/息屏、截图和无线镜像。
- 已接入 Wi-Fi Station、PGOS Studio、Xbox BLE 手柄、ES8311 播放/麦克风、板载 WS2812 RGB，以及 PCF8563 RTC。
- System Tools 已接入中文 `TinyLM`：5.69M 参数、2.95 MB INT4 模型在设备本地推理，提供预设故事开头、流式输出、A 重新生成和 B 返回列表。
- Games 菜单包含 Snake、Tetris、Breakout、Platformer、Blackjack、Minesweeper 和 2048。Platformer 目前包含 1-1 至 8-4 的 32 关战役。
- 外设由独立 Service 持有，应用只通过稳定接口访问能力；真实硬件结果和未完成项目以实验记录为准。

## 硬件基线

| 项目 | 当前配置 |
|---|---|
| 主控 | ESP32-S3 R8N16 |
| Flash / PSRAM | 16MB QIO / 8MB OPI |
| 屏幕 | ILI9341V，固件使用 320×240 横屏 |
| USB | 原生 USB CDC，GPIO19/20 |
| LCD | CS 10、MOSI 11、SCK 12、MISO 13、DC 46、背光 45 |
| RGB | WS2812，GPIO42 |
| 音频 / I²C | ES8311；共享 GPIO15/16 I²C 总线 |

完整引脚、电气边界和可实验扩展脚见[硬件基线](docs/HARDWARE.md)。

## 快速开始

需要 PlatformIO、Python 和一块已连接 USB 的开发板。

### 构建、烧录和串口监视

```powershell
pio run -e playground
pio run -e playground -t upload --upload-port COMx
pio device monitor --port COMx --baud 115200
```

### USB 控制台、截图和麦克风录音

```powershell
python -m pip install -r tools/requirements.txt
python tools/playground_console.py --list
python tools/playground_console.py --port COM3
python tools/capture_screen.py --port COM3 --output captures/home.png
python tools/capture_microphone.py --port COM3 --duration 3000 --output captures/mic-test.wav
```

控制台支持方向键、确认、返回、Home、页面直达、状态查询和无损 RGB565 截图；密码等敏感信息不会回显。常用页面示例：

```text
page system
page network
page mic
mic status
mic denoise on
page platformer
page minesweeper
page 2048
time status
gamepad status
```

完整命令和按键映射见[USB 控制台经验](docs/knowledge/04_USB_CDC_CONSOLE_AND_SCREENSHOT.md)。

### PGOS Studio

PGOS Studio 是用于无线遥控、状态查看、吞吐测试和屏幕镜像的轻量上位机。

```powershell
python -m pip install -r tools/requirements-studio.txt
python tools/pgos_studio.py --listen 0.0.0.0 --port 19000
```

首次连接可通过 USB 控制台保存上位机地址：

```text
server set <电脑IP> 19000
server status
```

控制、吞吐和镜像默认分别使用 TCP 19000、19001、19002。协议和当前限制见[PGOS Studio 实验记录](docs/experiments/008_pgos_studio_mirror.md)。

## 主机测试

```powershell
python -m unittest discover -s tools/tests -p "test_*.py"
```

主机测试覆盖游戏规则、输入策略、资源转换、协议工具和演示数据；真机验收仍需按对应实验记录执行。

TinyLM 的训练源码、词表和可再生资产位于 `src/third_party/esp32_ai/`；约 430 MB 原始 Parquet、训练中间文件和 `model.bin` 仅保存在本地并由嵌套 `.gitignore` 排除。当前模型需单独写入 Flash 的 `model` 分区，不能只上传应用固件。

## 文档入口

- [项目路线与当前进展](docs/PROJECT_PLAN.md)
- [PGOS 架构与模块边界](docs/ARCHITECTURE.md)
- [硬件和引脚基线](docs/HARDWARE.md)
- [Wi-Fi 与局域网方案](docs/WIFI_PLAN.md)
- [实验记录目录](docs/experiments/)
- [TinyLM 扩容与应用场景评估](docs/experiments/029_tinylm_scaling_and_applications_plan.md)
- [可复用经验知识库](docs/knowledge/README.md)
- [第三方依赖与源码策略](docs/DEPENDENCIES.md)
- [厂商资料说明](docs/vendor/README.md)
- [项目交接与开发规则](AGENTS.md)

## 目录概览

```text
main/       固件入口和 PGOS 实现
include/    公共头文件与资源接口
boards/     PlatformIO 自定义板卡定义
tools/      控制台、Studio、资源转换和主机测试
docs/       硬件基线、架构、路线和实验记录
captures/   屏幕截图与验收素材
```

Wi-Fi 凭据只保存在设备 NVS，不提交 Git。`docs/vendor` 是厂商参考资料，原始文件不应由实验代码修改；新增功能应先完成构建、主机测试和真机记录，再更新项目状态。
