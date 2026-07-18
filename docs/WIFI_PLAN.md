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
- SSID、密码和 token 不进入 Git；使用 `include/secrets.local.h` 或 NVS。
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
