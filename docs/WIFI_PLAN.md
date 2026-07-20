# Wi-Fi 与局域网通信计划

## 目标拓扑

```text
ESP32 Playground（STA）
          │ 2.4 GHz Wi-Fi
       家庭路由器
          │ 局域网
电脑 Python 上位机服务器
```

## 分阶段策略

第一步只做 Station 入网：连接、断线、重连、IP、RSSI 和 NTP。不要在第一版同时加入 WebSocket、MQTT、OTA 和复杂网页。

当前固件提供两条共用 `WifiService` 状态机的配网入口。日常入口是设备端 Network 页面：扫描 2.4 GHz 网络、方向键选择 SSID、加密网络进入 ASCII 软键盘、开放网络直接保存。USB CDC 控制台保留为维护和隐藏输入后备，不把目标 SSID 或密码写死在固件：

```text
wifi scan
wifi select <index>
wifi password <value>   # 设备和主机日志均不回显密码
wifi status
```

也可用 `wifi ssid <name>` 选择扫描不到的隐藏网络，或用 `wifi open` 配置开放网络。成功配置后，SSID 和密码写入 ESP32 NVS，重启后自动尝试连接；`wifi clear` 会删除已保存凭据并停止自动连接。

设备端软键盘使用 LVGL `textarea + keyboard/buttonmatrix`，但焦点移动继续由 PGOS 的 Up/Down/Left/Right/Confirm 语义驱动，不引入触摸依赖。密码限制沿用 WPA/WPA2 的 8～63 字节边界，输入框立即以 `*` 遮罩，密码不进入页面签名、日志或远程状态。前台 App 可优先消费 Back，因此密码页 Back 返回扫描列表，Home 仍由系统直接回桌面。

## 无线控制台方向

Wi-Fi 入网稳定后，USB 控制台的命令层可以复用为无线控制台协议。首选设备主动向电脑建立 TCP 长连接：设备只需知道电脑的地址和端口，不要求路由器把设备暴露为入站服务；连接断开后沿用现有退避状态机。UDP 广播或 mDNS 只用于局域网发现设备 ID、IP、固件版本和能力，不承载控制命令，也不能替代认证。

控制通道和性能测试通道分离。控制通道使用带长度或换行边界的命令帧，保留白名单、设备 ID 和 token；吞吐测试另开 TCP 流，使用固定大小缓冲区与伪随机数据，分别测上行、下行、往返延迟、持续时间和断线恢复。报告应用层有效吞吐（Mbps），同时记录 RSSI、路由器距离和测试时是否有其它流量，避免把链路层标称速率当成实测结果。

当前已加入第一版 `ServerService`：服务器地址、端口和启用状态保存在 `server` NVS namespace；`server set <host> <port>` 保存目标并自动启用连接，设备主动 TCP 连接服务器，发送 `PGOS/1 HELLO` 和周期 heartbeat，接收 `PING` 并回复 `PONG`。USB 与 TCP 命令共同进入有界 `InputRouter`；TCP 允许导航、页面、状态和受控吞吐测试白名单，返回 ACK/STATE，敏感 Wi-Fi/服务器配置命令被拒绝。电脑端日常使用 `tools/pgos_studio.py`，协议诊断继续使用无依赖的 `tools/pgos_server.py`，默认监听 TCP 19000。Studio 内置 19001 吞吐接收端，显示控制 RTT 与上下行 Mbps；当前尚未加入认证和 TLS，不作为通用远程 shell。

TCP 收包层使用 8 项有界命令队列，并在完整解析 `CMD <request_id> <command>` 后入队。队列满时必须使用原始 request ID 返回 `ACK <id> ERROR busy`，不能返回无法关联的固定 ID 0；随后再进入 16 项 `InputRouter` 队列。快速方向键输入因此可以按序确认，只有真实背压时才明确拒绝。

交互模式禁用 Arduino-ESP32 默认的 `WIFI_PS_MIN_MODEM`，避免控制 RTT 出现约 100 ms 的省电唤醒延迟；代价是 Wi-Fi 功耗上升。未来电池模式应把低延迟/省电作为显式电源策略，而不是隐式切换。

独立的 `BenchmarkService` 使用 TCP 19001 测量固定字节流，不与控制通道混用。PGOS Studio 已内置非阻塞测速接收端，命令行仍可使用 `tools/pgos_benchmark.py`；设备命令为 `bench upload <bytes>`、`bench download <bytes>`、`bench status` 和 `bench cancel`。

`MirrorService` 使用 TCP 19002（控制端口 + 2）发送 `PGMF` 二进制帧，帧负载为 320×240 RGB565BE shadow framebuffer。镜像默认关闭，由已连接的 PGOS Studio 通过白名单命令临时打开，不写入 NVS；控制会话不可用时镜像 socket 关闭。第一版发送完整帧并限制单轮发送预算，优先验证链路、颜色和交互延迟，再优化脏矩形与压缩。

端口拆分只用于当前实验隔离。真机确认各通道后，可把日常控制、镜像、音频和文件连接统一接入 TCP 19000，由首帧声明 channel；每个 channel 仍使用独立连接。BenchmarkService 可继续保留独立测试端口，避免测试工具与日常协议互相约束。

下一步发现层使用 UDP 19003：设备入网或控制连接失败时发送有界 `DISCOVER`，Studio 启动后一段时间发送低频 `BEACON`，匹配后立即触发现有 TCP 主动连接。发现响应携带协议版本、`server_id`、名称、控制端口和能力；首次出现多个服务器时由用户选择，后续只自动接受已配对的 `server_id`。UDP/mDNS 只负责发现和唤醒，不替代 TCP、认证或退避重连。

第二步使用 HTTP + JSON 完成双向联调：

```text
GET  /api/v1/status
GET  /api/v1/telemetry
POST /api/v1/command
POST PC_SERVER/api/v1/heartbeat
```

对于单块开发板和一台电脑，REST 足够直观，也方便用浏览器、PowerShell 和 Python 调试。未来需要多设备消息分发时再引入 MQTT；需要浏览器实时刷新时再增加 WebSocket。

## 运行约束

- ESP32-S3 只使用 2.4 GHz Wi-Fi，路由器必须确认该频段可用。
- `WiFi.begin()`、重连和 HTTP 请求都必须有超时，不能阻塞屏幕、USB、按键或音频。
- 网络状态通过事件或状态快照传给 UI，不能让 Wi-Fi 回调直接绘图。
- 使用 DHCP 起步，并同时显示 IP；mDNS 只能作为便利入口，不能替代 IP。
- 设备只暴露在家庭局域网，命令接口增加 token 和白名单。
- SSID、密码和 token 不进入 Git；本实验使用设备 NVS，主机侧配网脚本通过 `getpass` 隐藏密码输入。
- 心跳建议从 2～5 秒开始，失败后退避重试，不做高频刷包。

## 建议的状态字段

```json
{
  "schema": 1,
  "device_id": "es3n28p-01",
  "firmware": "0.3.0",
  "uptime_ms": 123456,
  "wifi": {
    "ssid": "home",
    "ip": "192.168.1.42",
    "rssi": -50
  },
  "memory": {
    "free_heap": 123456,
    "free_psram": 7000000
  }
}
```

字段结构先稳定，再增加设备能力和实验数据；不要把未经验证的业务字段直接带入 Playground 协议。
