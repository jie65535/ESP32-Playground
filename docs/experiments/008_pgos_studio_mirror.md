# 实验 008：PGOS Studio 与无线屏幕镜像

> 日期：2026-07-18，2026-08-02 回归修复
> 状态：控制、地址键盘与完整帧镜像已完成真机短测，长期稳定性待验收

## 目标

- 让设备在 Wi-Fi Station 模式下只保存并主动连接目标上位机。
- 上位机不占用 USB CDC，即可用方向、确认、返回和 Home 控制当前设备 UI。
- 上位机显示设备 ID、IP、RSSI、heap 和有界事件日志。
- 上位机测量控制 RTT、镜像实时 FPS/应用层 Mbps，并提供上下行吞吐测试。
- 用独立 TCP 通道实时显示设备的 320×240 shadow framebuffer。
- 不在设备菜单中暴露镜像、画面推送或音频流开关。

## 架构

```text
ESP32 ServerService  ── TCP 19000 ──> PGOS Studio 控制/状态
ESP32 BenchmarkService ─ TCP 19001 ─> PGOS Studio 吞吐接收端
ESP32 MirrorService  ── TCP 19002 ──> PGOS Studio RGB565 镜像
```

显示端仅在镜像 socket 已连接时维护 shadow framebuffer；断开镜像后不再为完整帧
协议支付每次 LVGL flush 的 PSRAM 拷贝成本。连接建立时先强制完整 keyframe。

PGOS Studio 的界面只提供语义输入，不保存页面清单，也不提供应用深链接。`page ...` 命令继续保留在 USB 控制台和协议诊断工具中。

## 镜像帧格式

每帧使用 22 字节大端帧头：

| Offset | Size | Field |
|---:|---:|---|
| 0 | 4 | ASCII `PGMF` |
| 4 | 1 | version = 1 |
| 5 | 1 | type = 1（RGB565BE） |
| 6 | 2 | width |
| 8 | 2 | height |
| 10 | 4 | payload bytes |
| 14 | 4 | frame id |
| 18 | 4 | device uptime ms |

帧负载当前固定为 320×240×2 = 153600 字节。固件目标约 5 FPS，每次主循环最多发送 8192 字节，避免一次完整帧阻塞 UI 与控制输入。镜像开关只存在于当前运行会话，不写入 NVS。

## 主机运行

```powershell
python -m pip install -r tools/requirements-studio.txt
python tools/pgos_studio.py --listen 0.0.0.0 --port 19000
```

首次使用可在设备的 Settings -> 远程控制中打开软键盘，输入电脑 IP
或主机名并保存。USB 控制台继续作为维护后备：

```text
server set 192.168.1.4 19000
server status
```

`server set` 会自动启用连接。Studio 收到设备后可直接用键盘方向键、Enter、Backspace/Escape 和 Home 导航；点击“开始镜像”后设备才建立 19002 镜像连接。

## 本轮验证

- `python -m py_compile tools/pgos_studio.py tools/playground_console.py tools/pgos_server.py`
- `python -m unittest discover tools\tests`：13 项通过。
- `pio run -e playground`：通过。
- 构建资源：RAM 51488 / 327680 bytes（15.7%）；Flash 1131889 / 6553600 bytes（17.3%）。镜像另在 PSRAM 动态分配 153600 字节一致性快照缓冲。

## 2026-08-02 回归与修复

Wi-Fi、Xbox BLE 和 Studio 同时运行时，旧实现可能建立 19002 TCP 连接但一直没有
首帧，随后控制 RTT 上升到约 8 秒。根因是完整帧路径使用 `WiFiClient::write`；底层
发送缓冲紧张时会进行多轮秒级等待，阻塞系统主循环。修复包括：

- lwIP/Wi-Fi 大块分配优先使用 PSRAM，并为内部 DMA/协议分配保留 32 KiB。
- 镜像改用 `MSG_DONTWAIT` 分块发送；`EAGAIN`、`ENOMEM` 和 `ENOBUFS` 留到后续
  tick 重试，连续 3 秒无进度才断开并退避重连。
- 内部 heap 和最大连续块只作为 `mirror status` 遥测，不再用瞬时阈值拒绝启动。
- Studio 根据请求 ID 处理 ACK。首帧 15 秒仍未到达时只提示设备仍在重试，
  不再自动发送 `mirror off`；首帧到达后明确记录。
- Studio 的 QSS 字号统一使用 point 单位，消除启动时
  `QFont::setPointSize ... -1` 警告。

COM3 真机短测使用设备 `192.168.1.2`、Studio `192.168.1.4`，RSSI
`-26` 至 `-27 dBm`，Xbox `045E:0B13` 保持连接。Studio 发起镜像后约 1 秒收到
首帧，14 秒观察内接收帧从 1 增至 12，采样速率约 `0.5 FPS / 0.59 Mbps`；设备
同时报告 `stalls=0`、控制 RTT `149.9 ms`、单次主循环工作约 `2.4 ms`。进入服务器
地址编辑页时，`192.168.1.4` 正确预填且 ASCII 键盘可见，Back 返回不修改配置。
USB 截图会临时占用 shadow framebuffer，结束后镜像已自动重连。

本轮主机回归 70 项通过，固件构建与 COM3 烧录通过；当前构建使用 RAM
118864 / 327680 bytes（36.3%），Flash 2771957 / 4194304 bytes（66.1%）。

## 真机待测

- 控制连接断开和恢复时，镜像通道是否按预期关闭/重连。
- RGB565 颜色、方向和画面完整性的人工长时间观察。
- 不同 RSSI 下的帧率、控制延迟和长期运行稳定性。
- 镜像开启时是否影响 LVGL 动画、音频反馈或 Wi-Fi 控制响应。

真机结论应补充测试距离、RSSI、固件提交和主机命令；当前记录不能视为硬件实测完成。测速时要分别记录“镜像同时开启”和“镜像关闭”的结果，否则镜像流量会成为带宽测试的一部分。
