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

固件从当前 PSRAM 画布发送：

```text
PLAYGROUND_SCREENSHOT 1 320 240 RGB565BE 153600
<153600 bytes RGB565>
PLAYGROUND_SCREENSHOT_END
```

Python 工具把 RGB565 转换为 RGB888 PNG，并在 Windows 上写入 CF_DIB 剪贴板。截图不是相机照片，而是固件实际离屏画布。

## 并发陷阱

- 串口日志线程不能和截图读取同时消费同一条流。
- 截图前设置暂停事件，等待读取线程确认已经停下。
- 截图结束后清除暂停事件，恢复日志读取。
- 截图数据期间固件不能打印普通日志。
- 使用固定头、固定长度和固定尾，接收端必须校验 payload 长度。

## 常见失败

- 截图出现半张图：日志线程抢走了二进制数据。
- PNG 颜色错误：RGB565 字节序或 BGR/RGB 转换弄反。
- 串口一打开开发板就重启：DTR/RTS 没有关闭。
- 截图脚本占用串口导致控制台打不开：先关闭其它串口程序。
