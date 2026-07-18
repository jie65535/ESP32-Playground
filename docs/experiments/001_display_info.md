# 实验 001：屏幕与板卡信息

## 目标

确认 ES3N28P 的 ILI9341 横屏、背光和 USB CDC 最小链路正常，并建立后续实验的基线。

## 固件

- 环境：`playground`
- 构建依据：当前 Git 工作树
- 入口：`src/main.cpp`
- 串口：115200 baud，原生 USB CDC

## 预期现象

1. 上电后先保持背光关闭。
2. LCD 初始化并完成首帧后点亮背光。
3. 屏幕显示 `ESP32 Playground`、板型、屏幕、运行时间、空闲堆内存、空闲 PSRAM、Flash 和 Wi-Fi 计划状态。
4. 画面由 320×240 RGB565 离屏 Sprite 完整绘制后一次推送，不应看到清屏重绘闪烁。
5. 英文、数字、符号和中文统一由 Fusion BitmapFont 绘制，比例 advance 和 baseline 稳定。
6. USB 启动时输出基本信息，之后约每 10 秒输出一行状态，不应持续刷屏。

## 上板命令

```powershell
pio run -e playground
pio run -e playground -t upload --upload-port COMx
pio device monitor --port COMx --baud 115200
```

## 记录栏

- 测试日期：2026-07-18
- 开发板：ES3N28P / R8N16
- USB 端口：COM3
- 屏幕颜色和方向：RGB565 截图确认 320×240、BGR + inversion 配置正确
- 是否出现复位旧画面闪现：首帧背光关闭逻辑已实现，需继续观察不同复位方式
- 串口日志：单次启动信息，之后约每 10 秒状态；无重启循环
- 截图：系统页、显示页和控制台截图链路通过
- 结论：构建、烧录、字体渲染、键盘命令和无损截图均已通过当前样机验证
