# ESP32 Playground

面向 QD 电子 ES3N28P / ESP32-S3 R8N16 开发板的个人实验项目。

这个仓库不是产品固件，也不继承任何 BMS 面板、报警、RTC 历史或固定页面需求。它的目标是用小步实验把开发板上的显示、USB、GPIO、RGB、音频、I²C、Wi-Fi、局域网通信和其它外设逐个玩明白，并把真实接线和实测结果记录下来。

当前第一阶段是 v0.1：点亮 ILI9341 屏幕，显示芯片、内存、运行时间和板卡信息。下一阶段优先探索 Wi-Fi Station，连接家庭 2.4 GHz 路由器，并通过局域网与电脑上的 Python 服务器交换 JSON 状态。

v0.1 使用一个 320×240 RGB565 离屏 Sprite 在 PSRAM 中完成整帧绘制，再一次性推送到屏幕，避免清屏重绘闪烁；USB 状态日志只低频输出。

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

v0.1 不连接 BMS，不需要外部按键，也不需要 Wi-Fi 密码。USB 只输出启动和每秒状态日志。

## 文档入口

- [项目交接与工作规则](AGENTS.md)
- [硬件和引脚基线](docs/HARDWARE.md)
- [项目路线](docs/PROJECT_PLAN.md)
- [Wi-Fi 与局域网方案](docs/WIFI_PLAN.md)
- [实验记录：屏幕和板卡信息](docs/experiments/001_display_info.md)
- [厂商原始资料说明](docs/vendor/README.md)

原始厂商资料包位于 `docs/vendor/2.8inch_IPS_ESP32-S3_ILI9341V_ES3C28P_ES3N28P_V1.0`。该目录是参考资料，不是程序依赖；缺失时不应复制或修改其内容。
