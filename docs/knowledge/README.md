# ESP32 Playground 知识库

这里沉淀已经通过编译、截图、串口或真机观察验证过的可复用经验。每篇文章尽量包含：问题、现象、原因、解决方案、复现方法和适用边界。

## 显示与字体

1. [ILI9341 颜色校准](01_DISPLAY_COLOR_CALIBRATION.md)
2. [PSRAM 离屏渲染与首帧背光](02_OFFSCREEN_RENDERING_AND_FIRST_FRAME.md)
3. [像素字体生成与排版](03_PIXEL_FONT_PIPELINE.md)
4. [统一字体与动态数值](11_UNIFIED_FONT_AND_METRICS.md)

## USB 与调试

5. [USB CDC 控制台与无损截图](04_USB_CDC_CONSOLE_AND_SCREENSHOT.md)
6. [PlatformIO 自定义板型与构建隔离](05_PLATFORMIO_BOARD_AND_BUILD_ISOLATION.md)
7. [测试、烧录和真机验证闭环](10_TESTING_AND_REAL_DEVICE_VALIDATION.md)

## 外设与运行时

8. [ES8311 音频方向和功放](06_AUDIO_ES8311_AND_I2S_DIRECTION.md)
9. [共享 I²C 和可选硬件降级](07_SHARED_I2C_AND_OPTIONAL_HARDWARE.md)
10. [输入事件与非阻塞状态机](08_INPUT_EVENTS_AND_NONBLOCKING.md)
11. [Flash 持久化、CRC 和栈内存](09_PERSISTENCE_AND_STACK_MEMORY.md)

## 使用边界

知识库只沉淀开发板和工具经验。具体实验的数据模型、页面内容和通信协议应在本项目中重新定义，不要把未经验证的业务字段直接当成通用基线。
