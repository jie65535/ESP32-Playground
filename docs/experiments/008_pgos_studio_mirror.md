# 实验 008：PGOS Studio 与无线屏幕镜像

> 日期：2026-07-18  
> 状态：代码与主机构建已通过，等待真机验证

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

首次使用 USB 控制台配置电脑地址：

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

## 真机待测

- 控制连接断开和恢复时，镜像通道是否按预期关闭/重连。
- RGB565 颜色、方向和画面完整性。
- 实际帧率、控制延迟、RSSI 变化和长期运行稳定性。
- 镜像开启时是否影响 LVGL 动画、音频反馈或 Wi-Fi 控制响应。

真机结论应补充测试距离、RSSI、固件提交和主机命令；当前记录不能视为硬件实测完成。测速时要分别记录“镜像同时开启”和“镜像关闭”的结果，否则镜像流量会成为带宽测试的一部分。
