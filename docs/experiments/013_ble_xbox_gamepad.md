# 实验 013：Xbox Series 手柄 BLE HID 可行性研究

## 状态

- 日期：2026-07-19
- 阶段：已集成 PGOS、完成输入/震动/状态栏/独立页面与省电策略真机验证
- 生产源码：`BleGamepadService` + Controller App；COM3 当前运行 PGOS 固件

## 目标

确认 ES3N28P / ESP32-S3 能否通过蓝牙连接 Xbox 手柄，并评估它作为 PGOS 游戏输入设备的实现边界。

## 当前硬件证据

手柄先连接到 Windows PC。Windows PnP 返回：

- Friendly name：`Xbox Wireless Controller`
- VID/PID：`045E:0B13`
- 总线：`BTHLE`（Bluetooth LE）
- HID service：`0x1812`（Human Interface Device Service）
- Microsoft 驱动：`xinputhid.inf`，设备描述为 Bluetooth LE XINPUT compatible input device

Bluepad32 的 controller list 将 `045E:0B13` 标为 `Xbox Series X Controller (BLE)`。这对应 Xbox Series X|S 三按键手柄家族，通常为 Model 1914；最终外壳上的 Model 字样仍以电池仓标签为准。

## 结论

可行，但不是“所有 Xbox 手柄都能连”：

1. ESP32-S3 只有 Bluetooth LE，没有 Bluetooth Classic / BR/EDR。
2. 该手柄是 BLE HID，满足 ESP32-S3 的无线能力边界。
3. Bluepad32 已实现 Xbox Wireless 的 BLE 报文解析，支持 model 1914，以及更新到 BLE 固件的 model 1708，并提供摇杆、按键、扳机、断连和震动等 API。
4. 手柄必须作为 BLE 外设，ESP32 作为 central/HID host；普通“ESP32 BLE Server/UART”示例方向相反，不能直接复用。

旧 Xbox 手柄、仍使用 BR/EDR 的 Xbox 固件和未实现 Xbox 专有 HID 报文解析的通用 BLE HID 代码不在本结论范围内。Bluepad32 文档要求部分 Xbox 型号使用 v5.15 或更新固件；若探针能发现但不能完成连接，先用 Xbox Accessories 更新控制器固件。

## 与当前 PGOS 的关系

当前 `InputRouter` 已有 `InputSource::Ble`，但 `AppCommand` 只适合低频导航命令，不适合每秒几十次的摇杆状态。生产实现使用独立的 `BleGamepadService` 和固定大小的 `GamepadSnapshot`：

```text
Bluepad32 callback / HID host
        ↓  固定大小最新状态（不绘 UI、不分配 String）
BleGamepadService
        ├─ GamepadSnapshot：连接、按钮、D-pad、四轴、两扳机、电量
        ├─ edge events：按下/释放、连接/断开
        └─ rumble request：由游戏提交，服务调用手柄输出 API
        ↓
前台游戏 App（60 Hz 轮询 snapshot）
```

系统 Shell 只把 Menu/View/Guide 等少量按键映射为 `Home/Back/Confirm`；游戏 App 直接消费规范化的连续状态，不能把摇杆采样塞进 16 项 `InputRouter` 命令队列。

## 工具链边界

当前 `playground` 使用 pioarduino `55.03.39`、Arduino-ESP32 3.3.9、ESP-IDF 5.5.4、Bluepad32 4.2.0 和 BTstack 1.6.2。生产工程已经迁移为 ESP-IDF 主工程、Arduino 作为组件，并同时集成 Bluepad32 和 BTstack；它们不能当作普通 Arduino `lib_deps` 处理。

最初先做独立 `ble_gamepad_probe`：

- 单独的 ESP-IDF + Arduino component 构建环境
- 只验证扫描、配对、连接、报告解析、断连重连和震动
- 不加载 LVGL、TFT_eSPI、Wi-Fi 镜像或现有 PGOS 页面
- 通过后再评估把生产工程迁移到 Arduino-as-IDF-component，避免破坏已经验证的 LCD DMA、I²S、RMT、Wi-Fi 和 USB CDC

可复现的探针源码和 Kconfig fragment 保存在 `experiments/ble_gamepad_probe/`，外部 Bluepad32、BTstack 和 Arduino component 不复制进本仓库。

## 2026-07-19 编译与上板结果

已使用官方模板的 ESP32-S3 环境完成构建和 COM3 烧录：

- pioarduino platform：`54.03.21`
- ESP-IDF：`5.4.2`
- Bluepad32：`4.2.0`
- BTstack：`1.6.2`
- 探针 `firmware.bin`：589248 bytes
- ELF 静态 DRAM：`.dram0.data` 16660 bytes + `.dram0.bss` 46464 bytes
- 烧录器实测：ESP32-S3 revision v0.2、16MB Flash、8MB embedded OPI PSRAM、USB Serial/JTAG

原生 USB 启动日志确认：

```text
BR/EDR support: disabled
BLE support: enabled
BTstack up and running at 28:84:85:85:1C:DE
[probe] Bluepad32 Bluepad32 for Arduino v4.2.0
[probe] scanning for BLE gamepads; hold Xbox Pair for 3 seconds
BLE scan -> 1
```

启动过程中还出现一次 `opcode=0x0c05 status=1` 的 HCI warning，但 BLE 扫描随后成功进入状态 1，因此当前按非致命兼容性告警记录；若后续配对失败再单独定位，不能先把它升级为硬件故障。

当前手柄仍连接 Windows，未按背部 Pair 键，所以 20 秒观察窗口内没有连接回调。这不构成失败；首次连接仍需用户让手柄进入配对广播。

## PGOS 集成与省电策略（2026-07-19）

已加入以下生产能力：

- 状态栏蓝牙图标：连接时使用强调色，扫描时使用弱提示色，等待已绑定设备重连时使用暗色。
- Controller 独立页面：显示连接状态、型号、VID/PID、电量、扫描倒计时；提供配对扫描、震动测试、立即断开和空闲断开设置。
- USB 命令：`page controller`、`gamepad status`、`gamepad scan`、`gamepad stop`、`gamepad rumble`、`gamepad disconnect`。
- 空闲断开：默认 15 分钟，可选 Never / 5 / 15 / 30 分钟，保存在 `pgos_gamepad` NVS namespace。

扫描分成三种用途：

1. 启动时开放一次 60 秒配对窗口；Controller 页面或 `gamepad scan` 可再次开启，显式扫描结束后不保持永久扫描。
2. 连接成功后立即调用 `enableNewBluetoothConnections(false)` 停止发现扫描。
3. 意外掉线立即进入 10 秒重连扫描；主动断开或空闲断开先静默 30 秒，随后以 10 秒扫描 / 20 秒暂停的低占空比窗口寻找控制器。连接成功后立即停止窗口。
4. ESP32-S3 作为 BLE central，完全停扫时不能可靠发现重新开机的手柄。低占空比窗口也可能接受处于配对模式的新手柄；需要完整配对时间时使用页面或 `gamepad scan`。

Bluepad32 当前 BLE 扫描参数的 interval/window 都是 48 个 0.625 ms 单位，即扫描开启期间约为 30 ms / 30 ms 的连续窗口。配对扫描仍限制为显式的 60 秒；断线后的 10 秒扫描 / 20 秒暂停只在必要时短时开启射频，避免未连接时永久保持满占空比扫描。

## 真机验证结果

- 手柄：`XBox One`，VID/PID `045E:0B13`，Microsoft firmware `5.9.2709.0`。
- A/B/X/Y、D-pad、系统按键、双摇杆和双扳机数据已验证；系统导航有响应。
- 双马达震动已由用户确认有实际触感。
- 2026-07-19 当前固件启动后自动恢复绑定，连接后日志确认 `scan=off`。
- `gamepad status` 实测：`state=connected scan=off ... idle_ms=900000 scans=1 disconnects=0`，连续接收 908 个以上数据包。
- 初版空闲判断曾因连接回调的 `millis()` 晚于本轮 `nowMs` 数毫秒而发生无符号下溢，表现为刚连接立即断开；已改为有符号时间差并重新烧录，15 秒观察窗口内保持连接。
- 当前构建资源：RAM 76112 / 327680 bytes，Flash 1710297 / 6553600 bytes。
- 2026-07-20 源码修正：空闲活动改为带迟滞的语义输入状态，首次连接的固定模拟量偏移不会持续刷新计时；`gamepad status` 增加 `input_age_ms`、摇杆和扳机原始值。断线后增加有界自动重连窗口，真机验证尚待下次连接开发板时完成。

## 后续上板验证清单

1. 把空闲时间临时设为 5 分钟，完整等待一次自动断开，确认 `input_age_ms` 持续增长并随后进入 30 秒重连等待。
2. 在 30 秒等待、重连扫描和暂停三种阶段分别关闭再打开已绑定手柄：主动断开后的首次恢复应不晚于剩余 30 秒等待，进入周期窗口后应不晚于 20 秒暂停结束。
3. BLE 与 Wi-Fi STA + PGOS Studio 镜像同时运行时记录输入延迟、丢包、主循环 duty 和镜像 FPS。
4. 长时间观察 Xbox 电量报告是否更新，以及 `Unsupported gatt client event: 0x1d` 是否只是非致命通知兼容日志。

## 参考

- [Espressif ESP32-S3 产品页（Bluetooth 5 LE）](https://www.espressif.com/en/products/socs/esp32-s3)
- [ESP-IDF Bluetooth HID Host API](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/esp_hidh.html)
- [ESP-IDF ESP32-S3 RF coexistence](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/coexist.html)
- [Bluepad32 FAQ：ESP32-S3 只有 BLE、Xbox 固件协议差异](https://github.com/ricardoquesada/bluepad32/blob/main/docs/FAQ.md)
- [Bluepad32 支持的 Xbox 1708/1914 型号和 BLE 固件](https://github.com/ricardoquesada/bluepad32/blob/main/docs/supported_gamepads.md)
- [ESP-IDF + Arduino + Bluepad32 PlatformIO 模板](https://github.com/ricardoquesada/esp-idf-arduino-bluepad32-template)
