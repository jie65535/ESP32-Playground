# 实验 002：Wi-Fi Station 与 USB 配网

## 目标

在不把家庭网络凭据写入源码的前提下，验证 ESP32-S3 仅使用 2.4 GHz Station 入网，并通过屏幕和 USB CDC 显示连接状态、IP、RSSI、扫描结果和重连次数。

## 固件实现

- 入口：`src/main.cpp`，环境：`playground`。
- `WiFi.mode(WIFI_STA)`，连接由 `millis()` 驱动的状态机推进，不在主循环中等待 `WiFi.begin()` 完成。
- 连接超时为 15 秒；失败或断线后使用 1、2、4、8…秒退避，最大 30 秒。
- `Preferences` namespace 为 `wifi`，键名为 `ssid` 和 `pass`。密码只存在设备 NVS/RAM，不由日志打印。
- 网络页显示 State、SSID、IP、RSSI 和 Retry；`status` 命令输出同一份状态快照。

## USB 控制台配网

推荐使用主机工具的隐藏输入流程：

```powershell
python tools/playground_console.py --port COM3 --wifi-setup
```

也可以在任意 USB 串口终端逐行发送：

```text
wifi scan
wifi select <index>
wifi password <value>
wifi status
```

扫描结果会列出索引、SSID、RSSI 和信道；密码命令只报告“password hidden”。扫描不到的隐藏网络使用 `wifi ssid <name>`，开放网络使用 `wifi open`。删除设备上的凭据并停用自动连接：`wifi clear`。

## 预期现象

1. 无凭据首次启动时，网络页为 `no credentials`，USB 提示使用 `wifi scan`。
2. 配置保存后，网络页依次显示 `ready`、`connecting`，成功后显示 `connected`、DHCP IP 和 RSSI。
3. 路由器断电或离开覆盖范围时，状态转为 `backoff`，不会阻塞屏幕或 USB；恢复后自动重连。
4. `wifi status`、周期状态日志和屏幕使用同一组运行时快照，不显示密码。

## 上板验证

```powershell
pio run -e playground
pio run -e playground -t upload --upload-port COMx
python tools/playground_console.py --port COMx --wifi-setup
```

实测记录（当前样机）：

- 测试日期：2026-07-18
- 开发板/USB：ES3N28P / R8N16，COM3
- 路由器频段：未配置家庭凭据；扫描仅验证 2.4 GHz Station 能力
- 扫描网络数：25；第一次内部 6 秒窗口不足导致失败，放宽到约 12 秒后异步扫描通过
- DHCP IP、RSSI：待输入凭据后验证
- 断线与自动重连：待成功入网后验证
- 屏幕：网络页显示 `no credentials`、SSID/IP/RSSI 占位、Retry=0 和 `scan ready: 25 network(s)`
- 结论：USB 命令、NVS 空配置启动、非阻塞扫描、屏幕/USB 状态快照已通过；连接和重连待用户通过隐藏输入配置后继续。
