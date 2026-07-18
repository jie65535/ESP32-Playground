# 实验 001：屏幕与板卡信息

## 目标

确认 ES3N28P 的 ILI9341 横屏、背光和 USB CDC 最小链路正常，并建立后续实验的基线。

## 固件

- 环境：`playground`
- 版本：v0.1
- 入口：`src/main.cpp`
- 串口：115200 baud，原生 USB CDC

## 预期现象

1. 上电后先保持背光关闭。
2. LCD 初始化并完成首帧后点亮背光。
3. 屏幕显示 `ESP32 Playground`、板型、屏幕、运行时间、空闲堆内存、空闲 PSRAM、Flash 和 Wi-Fi 计划状态。
4. 画面由 320×240 RGB565 离屏 Sprite 完整绘制后一次推送，不应看到清屏重绘闪烁。
5. USB 启动时输出基本信息，之后约每 10 秒输出一行状态，不应持续刷屏。

## 上板命令

```powershell
pio run -e playground
pio run -e playground -t upload --upload-port COMx
pio device monitor --port COMx --baud 115200
```

## 记录栏

- 测试日期：待填写
- 开发板：ES3N28P / R8N16
- USB 端口：待填写
- 屏幕颜色和方向：待填写
- 是否出现复位旧画面闪现：待填写
- 串口日志：待填写
- 结论：待填写
