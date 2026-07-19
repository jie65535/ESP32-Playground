# ES3N28P / ESP32-S3 硬件基线

本文是 Playground 当前采用的板卡摘要。厂商原始资料仍以 `docs/vendor/2.8inch_IPS_ESP32-S3_ILI9341V_ES3C28P_ES3N28P_V1.0` 为准；原理图、数据手册和真机验证出现差异时，在实验记录中记录差异，不直接覆盖原始资料。

## 开发板

- QD 电子 2.8 英寸 ESP32-S3 无触摸版 ES3N28P
- ESP32-S3 R8N16
- 16MB QIO Flash
- 8MB OPI PSRAM
- ILI9341V，240×320 物理屏，固件使用 320×240 横屏
- 原生 USB CDC，无需额外 USB 转串口芯片

PlatformIO 使用仓库内 `boards/es3n28p_r8n16.json`，因为该板型不是 PlatformIO 的通用内置名称。这个定义是 Playground 自己维护的，不引用其它项目。

## 已确认引脚

| 功能 | GPIO | 说明 |
|---|---:|---|
| LCD CS | 10 | TFT 片选 |
| LCD MOSI | 11 | SPI 主出 |
| LCD SCK | 12 | SPI 时钟 |
| LCD MISO | 13 | 读屏时使用 |
| LCD DC | 46 | 数据/命令选择 |
| LCD RST | — | 与芯片复位共用，无独立 GPIO |
| LCD 背光 | 45 | 高电平点亮 |
| WS2812 RGB | 42 | 单线数据 |
| USB D-/D+ | 19/20 | 原生 USB |
| BOOT | 0 | 下载/启动，不作业务键 |
| I²C SCL/SDA | 15/16 | ES8311 及后续 I²C 外设 |
| 音频功放 EN | 1 | 低电平使能 |
| I²S MCLK/BCLK/WS | 4/5/7 | 板载 ES8311 |
| I²S DAC DATA | 8 | ESP32 → ES8311 播放 |
| I²S ADC DATA | 6 | ES8311 → ESP32 麦克风采集，待单独验证 |

## 外接 PCF8563 模块

- 用户已将带电池和晶振的 PCF8563 模块接入板载 I²C 总线；固件使用 7-bit 地址 `0x51`。
- SDA/SCL 复用 GPIO16/15，与 ES8311 地址 `0x18` 共存；固件由 `I2cBusService` 统一初始化 400 kHz 总线。
- 本项目只把 PCF8563 的 C 位按 `C=0 → 2000–2099` 使用，设置时间时写入 2000–2099 并清除 C 位。
- 上板前仍需确认模块 VCC 是 3.3 V 兼容、与开发板共地、板载上拉不会和 ES8311 并联成过低阻值，以及电池不是接入充电回路的不可充电 CR2032。

## 可实验扩展脚

GPIO2、GPIO3、GPIO14、GPIO21、GPIO43、GPIO44 可作为进一步扩展候选。使用前必须核对启动绑带、电平、外部上拉/下拉和板上连接。GPIO15/16 已保留给 I²C，GPIO45/46 已由屏幕占用。

## 屏幕配置

当前 `platformio.ini` 使用以下已验证设置：BGR 色序、ILI9341 inversion on、40MHz SPI、横屏旋转 1。背光在 LCD 初始化和首帧推送完成前保持关闭，减少复位时旧 GRAM 画面闪现。

## 电气安全

- 不把电池包总压直接接入开发板 5V、3.3V 或 GPIO。
- 外接模块先确认 3.3V 逻辑电平和共地方式。
- Wi-Fi 发射存在电流峰值，未来改用外部供电时需要为峰值留余量。
- 厂商目录中的 EXE、APK、烧录器和压缩包不自动运行。
