# 实验 009：运行时监视与 TCP 命令队列

> 日期：2026-07-18  
> 状态：代码与构建通过，等待真机验证

## 背景

完整 320×240 RGB565 帧为 153600 字节，即 1.2288 Mbit。实测独立链路约 4.6 Mbps，理论完整帧上限约 3.7 FPS；镜像约 2.6 Mbps，对应约 2.1 FPS。网络已经构成一部分上限，但仍需观察主循环、内存和任务状态定位剩余损失。

快速连续发送方向键时曾出现：

```text
ACK 0 ERROR busy
```

协议帧本身携带 request ID，错误来自 `ServerService` 的单命令 pending 槽在解析 ID 前提前拒绝请求。

## 本轮实现

- `RuntimeMonitorService` 每秒计算一次主循环工作时间占墙钟时间的比例。
- 主循环末尾 `delay(1)`，为 FreeRTOS idle、Wi-Fi 和系统任务提供稳定调度窗口。
- System 页面改为可滚动监视器，显示：
  - Main loop duty
  - Uptime
  - Free/total Heap
  - Minimum free Heap
  - Free/total PSRAM
  - Sketch/Flash
  - OTA free space
  - FreeRTOS task count
- Wi-Fi Station 禁用默认 modem sleep，优先降低交互 RTT。
- TCP 接收层从单 pending 槽改为 8 项 FIFO；队列满时以原始 request ID 返回 `busy`。
- TCP FIFO 之后仍进入 16 项 `InputRouter`，两层都保持有界。

## CPU 指标边界

当前 Arduino-ESP32 预编译 FreeRTOS 配置没有开启完整 runtime stats，因此页面的 `MAIN CPU` 实际表示 PGOS 主循环 duty，不代表 ESP32-S3 两个核心的总 CPU 利用率。它足以判断镜像发送、渲染或命令处理是否持续占满主循环，但不能统计 Wi-Fi driver 等其它核心任务。

完整双核任务统计应在后续切换可控 sdkconfig 后，通过 FreeRTOS idle/runtime counter 实现；在此之前不显示伪精确百分比。

## 验证

- `pio run -e playground`：通过。
- `python -m unittest discover tools\tests`：13 项通过。
- 构建资源：RAM 51896 / 327680 bytes（15.8%）；Flash 1136117 / 6553600 bytes（17.3%）。

## 真机待测

- 镜像关闭与开启时分别记录 RTT、主循环 duty、镜像 FPS/Mbps。
- 连续快速发送 20 次方向键，确认 ACK 均带对应 request ID，或在真实队列满时按原 ID 返回 busy。
- 观察最低 Heap 是否随镜像、测速和页面滚动持续下降。
- 对比 modem sleep 关闭前后的 RTT、RSSI、吞吐和功耗。
