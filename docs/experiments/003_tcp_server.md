# 实验 003：设备主动 TCP 诊断连接

## 目标

在 Wi-Fi Station 已入网的基础上，让 PlaygroundOS 主动连接电脑服务器，验证可配置目标、TCP 长连接、HELLO、heartbeat、PING/PONG 和断线退避。

## 拓扑

```text
PGOS / ESP32-S3  192.168.1.6
          │ TCP 19000
PC Server         192.168.1.4
```

## 设备配置

```text
server set 192.168.1.4 19000
server on
server status
```

服务器目标、端口和启用状态保存在 `server` NVS namespace；地址没有写死在固件。

## 电脑服务器

```powershell
python tools/pgos_server.py --listen 0.0.0.0 --port 19000
```

当前服务器不依赖第三方 Python 包。它接收 `PGOS/1 HELLO` 和 JSON heartbeat，每 10 秒发送一次 `PING`，设备回复 `PONG`。

## 实测记录

- 日期：2026-07-18
- 设备 IP：192.168.1.6
- 电脑 IP：192.168.1.4
- TCP 端口：19000
- HELLO：通过，包含 device_id、固件版本和设备 IP
- heartbeat：通过，包含 uptime、IP、RSSI 和 free heap
- PING/PONG：通过
- 远程白名单命令：`page display` 通过并返回 `ACK 1 OK accepted`
- 状态请求：`status` 通过并返回 `STATE`，包含当前 App、Wi-Fi 状态、IP、RSSI 和 heap
- 敏感命令：`wifi clear` 被拒绝，返回 `ACK 3 ERROR command_not_allowed`
- 输入路由：USB CDC 与 TCP 均进入同一个有界 `InputRouter`
- 设备状态：`server state=connected`，重连次数初次连接为 0
- 断开服务器：设备进入退避并尝试重连；测试结束后执行 `server off` 保留目标但停止重试

## 安全边界

当前链路没有认证或 TLS；只接受硬编码白名单中的导航、页面和状态命令，不接受任意系统命令。下一阶段继续补充正式 Session、控制权和配对/token。
