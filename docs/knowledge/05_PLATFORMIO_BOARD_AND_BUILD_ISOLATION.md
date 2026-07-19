# PlatformIO 自定义板型与构建隔离

## 自定义板型

`es3n28p_r8n16` 不是 PlatformIO 内置板型，必须在仓库的 `boards/es3n28p_r8n16.json` 中声明：

- ESP32-S3 R8N16
- `qio_opi` 内存类型
- 16MB 分区
- 原生 USB CDC 编译宏
- 16MB Flash 和自定义硬件 ID

这样新项目不需要引用另一个仓库的 `boards` 目录。

## 固件配置

LCD 引脚、SPI host/clock 和 `esp_lcd_ili9341` 依赖由仓库内板型、CMake 与锁文件共同声明，不修改用户全局库文件。板型定义、屏幕驱动、应用代码和实验环境都在同一个仓库可复现。

## 构建隔离

以后增加 Wi-Fi、音频或实验页面时，建议为每个实验提供独立环境或编译开关；正式构建不能意外带入测试命令、高频日志和模拟入口。构建结束后检查固件字符串和资源边界，而不是只看“编译成功”。

## Windows 增量构建经验

ESP-IDF 5.5.4 即使 release 构建也会为组件加入 `-gdwarf-4 -ggdb`。这些调试段不进入烧录的 app binary，却会让本地 ELF 达到约 18.4 MB；移除后约为 3.4 MB，并显著降低 Windows 链接和文件扫描成本。当前 `platformio.ini` 用 `build_unflags` 在常规 `playground` 构建移除它们；需要源码级 GDB 行号时临时注释这两项即可。

pioarduino 的 ESP-IDF/SCons 混合层还存在一个增量依赖陷阱：项目源对象可能重新编译，但最终 ELF 不一定被标记为过期。`tools/platformio_incremental_guard.py` 在构建阶段比较 `src/`、`include/`、`main/` 输入和 ELF 时间戳，仅在输入较新时强制重链，避免“烧录了旧固件”的假成功。

在当前 Windows 主机上的实测：

- 干净重建（全组件）：约 350 秒，一次性成本；
- 无改动 `pio run -e playground`：约 17 秒，不再无意义重链；
- 只改一个业务源文件并实际改变固件字符串：约 37 秒，ELF、BIN 时间戳和 BIN SHA-256 都更新；
- 旧配置的无改动链接通常约 130–137 秒。

修改 `main/CMakeLists.txt` 或新增源文件仍可能触发 CMake 重新配置；这是源集合变化，不应和普通业务编辑的增量编译混为一谈。若一次构建被外部超时强行终止，先等待残留 `pio/python/ninja` 进程退出，再清理工作区内的 `.pio/build` 后重建，避免 Windows 文件锁留下不完整的 CMake API 元数据。
