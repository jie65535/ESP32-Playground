# 实验 004：TCP 应用层吞吐

## 目标

在独立 TCP 流上测量 ESP32-S3 与电脑之间的应用层有效吞吐，避免把大流量混入 PGOS 控制通道。

## 端口和命令

- 控制通道：TCP 19000
- 吞吐通道：TCP 19001
- 测试数据：固定伪随机字节，不保存到 Flash，不进入 NVS

电脑端启动：

```powershell
python tools/pgos_benchmark.py --listen 0.0.0.0 --port 19001
```

设备端目标沿用 `server` NVS 中的主机地址：

```text
bench upload 4194304
bench download 4194304
bench status
bench cancel
```

## 测量口径

吞吐为应用层有效字节数 × 8 / 传输耗时，单位 Mbps。TCP/IP、Wi‑Fi MAC、协议头和 Python/Arduino 缓冲开销不计入有效载荷，因此不等同于芯片或路由器标称 PHY 速率。

## 实测记录

- 日期：2026-07-18
- 设备：ESP32-S3 / `192.168.1.6`
- 电脑：`192.168.1.4`
- RSSI：约 -16 到 -18 dBm
- 数据量：4 MiB（4,194,304 bytes）
- ESP32 → PC：5.73 Mbps，5,857,242 µs
- PC → ESP32：6.03 Mbps，5,562,174 µs
- 控制通道：测试期间保持独立；屏幕和 USB 状态继续刷新

## 解释和后续

该结果是当前固件缓冲区、Arduino WiFiClient、电脑 Python socket、路由器信道和距离组合下的应用层结果，不是硬件极限。后续可以比较缓冲区大小、Wi‑Fi 省电模式、RSSI、距离、持续时间和并发控制流，并增加 RTT、抖动和丢线恢复测试。
