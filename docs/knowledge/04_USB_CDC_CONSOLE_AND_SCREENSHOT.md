# USB CDC 控制台、键盘控制与无损截图

## USB 串口打开方式

ESP32-S3 原生 USB Serial/JTAG 使用 USB CDC。Windows 上打开串口前必须先把 pyserial 的 DTR/RTS 设为 `False`，否则可能触发芯片复位或进入 ROM 下载器。

## 控制台结构

`tools/playground_console.py` 分成三层：

1. 键盘层：数字键、字符快捷键和可选的方向键/回车。
2. 传输层：dry-run 或真实串口。
3. 设备命令层：`up`、`down`、`ok`、`status`、`screenshot` 等换行命令。

Playground 不要求外接业务按键；键盘快捷键和完整命令直接进入设备命令入口。以后增加 Wi-Fi 命令时，继续沿用同一套命令协议。

## 无损截图协议

固件从当前 PSRAM 画布发送版本 2 二进制帧：

```text
PGS2 header (24 bytes: version/format/size/request_id/sequence)
<153600 bytes RGB565BE>
PGE2 trailer (request_id/sequence/CRC32)
```

Python 工具把 RGB565 转换为 RGB888 PNG，并在 Windows 上写入 CF_DIB 剪贴板。截图不是相机照片，而是固件实际离屏画布。

## 并发陷阱

- 串口日志线程不能和截图读取同时消费同一条流。
- 截图前设置暂停事件，等待读取线程确认已经停下。
- 截图结束后清除暂停事件，恢复日志读取。
- 截图数据期间固件不能打印普通日志。
- 使用固定 magic、固定长度和固定尾，接收端必须校验 request、payload
  长度、sequence 和 CRC32。

2026-07-19 截图接收加固：

- 主机默认完整帧超时提高到 30 秒；153600 字节在名义 115200 baud 下仅
  传输时间就超过 13 秒。
- 主机按 `PGS2` magic 在原始字节流中重同步，遇到残留 RGB565 或旧
  request 时继续扫描，不再对二进制帧调用 `readline()`。
- 设备端单个 CDC chunk 连续阻塞超过 3 秒会放弃当前截图，避免主循环
  永久卡在已断开的主机上。
- 截图 trailer 附带 payload CRC32；主机在 PNG 转换前校验，能把传输截断
  或混帧明确报告为 CRC 错误，而不是生成看似正常的坏图。
- 这仍是 USB CDC 调试通道的加固；长期应把截图迁移到独立 BulkData/USB
  endpoint，并补充发送完成 ACK 和显式 abort。

## 常见失败

- 截图出现半张图：日志线程抢走了二进制数据。
- PNG 颜色错误：RGB565 字节序或 BGR/RGB 转换弄反。
- 串口一打开开发板就重启：DTR/RTS 没有关闭。
- 截图脚本占用串口导致控制台打不开：先关闭其它串口程序。
- 截图尾部缺少几十到几百字节：通常是主机超时关闭，设备仍在发送旧帧；
  下一次接收必须能够跳过残留二进制。
