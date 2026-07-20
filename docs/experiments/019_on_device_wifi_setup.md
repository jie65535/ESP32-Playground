# 实验 019：设备端 Wi-Fi 扫描与方向键软键盘

## 目标

让 ES3N28P 无触摸版在没有 PGOS Studio 或 USB 隐藏输入工具时，也能完成常见的 2.4 GHz Wi-Fi 配网：设备端启动扫描、方向键选择 SSID、输入 WPA/WPA2 密码并保存到既有 `wifi` NVS namespace。

## 实现

- Network 页面从状态展示扩展为三个可操作项：Wi-Fi 电源、配置网络、重新连接。
- 配置网络沿用 `WifiService` 的非阻塞异步扫描和重试状态机，不在 UI 中等待 `WiFi.scanNetworks()`。
- 扫描列表显示 SSID、RSSI 和开放网络标记；开放网络跳过密码页并直接保存。
- 加密网络进入 `OnScreenKeyboard`。组件使用 LVGL 9.5 的 `lv_textarea`、`lv_keyboard` 和 `lv_buttonmatrix`，提供小写、大写、数字/符号三套布局并覆盖完整可打印 ASCII。
- 方向键移动键位焦点，Confirm 输入当前键；Xbox `X` 在小写、大写和符号页之间快速切换。
- 密码长度限制为 8～63 字节，立即使用 `*` 遮罩；密码不写入日志、页面签名或远程状态。
- `IApp::onBack()` 允许密码页先消费 Back 并返回扫描列表；普通页面 Back 和全局 Home 语义不变。
- USB 的 `wifi scan/select/password/open` 命令、NVS schema 和自动重连行为保持兼容。

## 自动验证

```powershell
python -m unittest discover -s tools/tests -p "test_*.py" -v
pio run -e playground
```

2026-07-20 当前结果：

- 24 项主机测试通过，其中新增 3 项检查 LVGL 依赖开关、完整可打印 ASCII 覆盖和密码即时遮罩。
- 固件构建通过：RAM 79,472 / 327,680 bytes，Flash 1,799,629 / 6,553,600 bytes。
- LVGL 键盘、文本框和 button matrix 已进入链接镜像；相对上一版 RAM 增加约 136 bytes，Flash 增加约 19.8 KB。

## 真机检查

烧录后按以下顺序验证，结果必须区分“页面可操作”和“真实凭据连接成功”：

1. 设置 → 无线网络，确认三个操作项、状态卡和滚动焦点没有重叠。
2. 配置网络，确认扫描期间屏幕、USB、BLE 和后台服务保持响应。
3. 在列表中上下移动并进入一个加密 SSID，检查小写、大写、符号页、退格、空格和密码遮罩。
4. 在密码页按 Back 返回原扫描列表；按 Home 直接回桌面。
5. 输入少于 8 个字符并确认，检查错误提示且不离开输入页。
6. 使用真实密码完成保存、连接、DHCP、RSSI 和重启自动恢复。
7. 选择开放网络时确认不出现密码页；当前环境没有可信开放 AP 时不要为测试临时降低家庭路由器安全性。

真实密码连接和重启自动恢复已通过；错误密码退避、开放网络和长期稳定性仍待
后续真机验证。

## 首次真实密码连接与重启问题（2026-07-20）

- 设备端扫描、SSID 选择、密码遮罩输入和凭据保存均完成，Wi-Fi 能成功关联
  `ChinaNet-3wr6`，部分启动中 DHCP 已取得 `192.168.1.6`。
- 随后 FreeRTOS 明确报告 `A stack overflow in task sys_evt has been detected`，
  设备进入重启循环。故障点是 ESP-IDF 默认系统事件循环，不是密码校验失败，
  PGOS 也没有在 Wi-Fi 事件回调中执行 SNTP、Server 或 LVGL 工作；这些路径仍由
  `loopTask` 推进。
- 当前 ESP-IDF 5.5.4 配置给 `sys_evt` 的栈为 2304 bytes。项目已在
  `sdkconfig.defaults` 显式提高到 4096 bytes，增加 1792 bytes 内部 RAM 开销；
  设备凭据保持不变。
- 修复后 `pio run -e playground` 通过，生成的 `sdkconfig.h` 已确认值为 4096；
  最终烧录构建 RAM 79,456 / 327,680 bytes，Flash 1,799,109 / 6,553,600 bytes，Python 26 项
  测试通过。
- 修复版本已烧录 COM3，上传未擦除 NVS。设备自动恢复保存的 SSID，状态为
  `connected`，DHCP IP `192.168.1.6`，RSSI `-39` 至 `-38 dBm`，重连次数 0；
  PCF8563 为 `ready`，SNTP 为 `synced`、同步次数 1，Xbox 手柄仍保持连接。
- 烧录后首轮 45 秒连续监听没有再次出现 ROM 启动头、panic 或 `sys_evt` 栈溢出，
  用户随后确认设备运行正常并停止延长观察。因此本次重启故障判定为已修复；
  Server 未配置目标，本轮无法验证自动连接，至少 10 分钟及更长期稳定性仍保留
  为后续检查项。
