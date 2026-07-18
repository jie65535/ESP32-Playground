# ESP32 Playground

面向 QD 电子 ES3N28P / ESP32-S3 R8N16 开发板的个人实验项目。

这是一个面向 ESP32-S3 开发板的个人实验空间。目标是用小步实验把显示、USB、GPIO、RGB、音频、I²C、Wi-Fi、局域网通信和其它外设逐个玩明白，并把真实接线和实测结果记录下来。

当前固件点亮 ILI9341 屏幕，显示芯片、内存、运行时间和板卡信息。下一步优先探索 Wi-Fi Station，连接家庭 2.4 GHz 路由器，并通过局域网与电脑上的 Python 服务器交换 JSON 状态。

当前固件使用一个 320×240 RGB565 离屏 Sprite 在 PSRAM 中完成整帧绘制，再一次性推送到屏幕，避免清屏重绘闪烁；USB 状态日志只低频输出。

## 当前环境

- PlatformIO + Arduino
- `espressif32 @ 7.0.1`
- `es3n28p_r8n16`
- ESP32-S3，16MB QIO Flash，8MB OPI PSRAM
- ILI9341V，逻辑分辨率 320×240 横屏
- 原生 USB CDC，`Serial` 波特率 115200

## 第一个固件

```powershell
pio run -e playground
pio run -e playground -t upload --upload-port COMx
pio device monitor --port COMx --baud 115200
```

当前固件不连接外部模块，也不需要 Wi-Fi 密码。USB 输出启动信息和低频状态日志。

## USB 控制台和截图

安装依赖后可使用箭头键、回车、快捷键和无损截图：

```powershell
python -m pip install -r tools/requirements.txt
python tools/playground_console.py --list
python tools/playground_console.py --port COM3
python tools/capture_screen.py --port COM3 --output captures/home.png
```

控制台的 `1/2/3` 可直接跳页，`C` 显示色卡，`R` 查询状态，`S` 会暂停日志读取线程并导出 PSRAM 中的原始 RGB565 画布到 PNG/Windows 剪贴板。方向键和回车只是可选便利，`Q` 退出控制台。

## 文档入口

- [项目交接与工作规则](AGENTS.md)
- [硬件和引脚基线](docs/HARDWARE.md)
- [项目路线](docs/PROJECT_PLAN.md)
- [Wi-Fi 与局域网方案](docs/WIFI_PLAN.md)
- [实验记录：屏幕和板卡信息](docs/experiments/001_display_info.md)
- [可复用经验知识库](docs/knowledge/README.md)
- [厂商原始资料说明](docs/vendor/README.md)

原始厂商资料包位于 `docs/vendor/2.8inch_IPS_ESP32-S3_ILI9341V_ES3C28P_ES3N28P_V1.0`。该目录是参考资料，不是程序依赖；缺失时不应复制或修改其内容。
