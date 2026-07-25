# ESP32-S3 PLE TinyLM 运行时真机验证

> 日期：2026-07-25
> 状态：完整随机权重推理链真机通过；语言质量与作者原始模型性能未验证
> 硬件：QD ES3N28P，ESP32-S3 rev 0.2，16MB Flash，8MB OPI PSRAM
> PGOS 基线：`1a97eb0`
> 上游：`slvDev/esp32-ai`，提交 `9c4a214`

## 目标与边界

验证当前 ES3N28P 是否能够执行 `esp32-ai` 仓库的完整 PLE TinyLM 推理路径，
包括模型 Flash 映射、int4 权重读取、PSRAM 中的 int8 输出头、双核输出头、
PLE、注意力、FFN、KV cache 和逐 token 解码。

上游没有发布其训练权重、tokenizer 和 `vocab.h`，因此本实验不能验证故事质量，
也不能复现作者公布 SHA-256 对应模型的文本。实验使用同尺寸、同布局的确定性
测试权重；生成的 token ID 没有语言意义。

上游仓库没有 LICENSE。验证工程仅保存在被 Git 忽略的
`artifacts/esp32-ai-validation/work/llm_probe/`，构建时直接引用本机上游
`firmware/common/llm.h`，没有把上游源码复制进 PGOS Git 历史。

## 验证配置

- 构建环境：现有 PlatformIO / ESP-IDF 5.5.4，不安装 Arduino 框架。
- 设备参数：240MHz 双核 LX7、16MB Flash、8MB OPI PSRAM 80MHz。
- 模型配置：`V=32768, D=96, L=6, H=4, F=66, P=128, S=256, G=128`。
- 设备端有效词表行数：25,353，与上游当前 `vocab.h` 生成逻辑和输出头裁剪一致。
- 模型分区：offset `0x110000`，size `0xEE0000`。
- 测试模型大小：14,912,332 bytes，与作者 README 声明的模型大小一致。
- 测试模型 SHA-256：
  `e902c8bac94fb1141ecf90a55713efb6aa0c7bfead4ac32d9638a83a2b8ec7ed`。
- 测试权重：int4 code 使用固定伪随机的 `-1/0/1`，fp16 scale 为 `0.02`，
  RMSNorm 权重为 `1.0`。
- 输出：关闭 LCD 和词表解码，只通过 USB Serial/JTAG 输出 token ID 与 profile。
- 编译：推理组件命令行的最后一个优化选项为 `-O3`。

虽然下载镜像头由 esptool 以 DIO 模式写入，探针的二阶段启动日志明确报告：

```text
qio_mode: Enabling default flash chip QIO
Boot SPI Speed : 80MHz
SPI Mode       : QIO
spi_flash: flash io: qio
```

因此下列探针数据来自实际 QIO 80MHz Flash 运行态。

## 安全措施和烧录

测试前检查 COM3 的 USB VID/PID，并读取芯片、Flash 和 eFuse：

- ESP32-S3 rev 0.2；
- 16MB Flash；
- 8MB embedded OPI PSRAM；
- secure boot 和 flash encryption 均未启用。

整片 Flash 连续读取在旧应用区域 `0x18D000` 附近稳定触发 USB 下载传输中断，
因此没有把不完整文件当作备份。改为分别备份不可重建的数据区：

- bootloader / partition / NVS 前 64KB；
- OTA data；
- 3.375MiB SPIFFS；
- coredump。

PGOS 应用由当前源码重新构建。探针固件、分区表和 14.9MB 模型均由 esptool
完成写入哈希校验。

## 真机结果

设备成功解析模型头、展开输出头并完成 4 个提示 token 加 200 个生成 token 的
连续前向推理：

| 项目 | 实测 |
|---|---:|
| PSRAM 总量 | 8192 KB |
| 推理初始化前空闲 PSRAM | 8189 KB |
| int8 输出头 | 2.54 MB，25,353 × 96 |
| 完成全部分配后的空闲 PSRAM | 4364 KB |
| 完整前向次数 | 200 |
| 总时间 | 35.660 s |
| 端到端吞吐 | 5.61 token/s |
| compute-only 吞吐 | 5.76 token/s |
| compute-only 延迟 | 173.57 ms/token |
| input | 6.79 ms/token |
| attention | 37.13 ms/token |
| FFN | 10.60 ms/token |
| PLE | 13.36 ms/token |
| output head | 105.68 ms/token |

最终串口结论：

```text
[probe] PASS: complete local Transformer inference executed
[probe] test weights are deterministic and untrained; token IDs have no language meaning
```

设备没有复位、崩溃、模型 magic 错误或 PSRAM 分配失败。这证明当前开发板能够
执行该仓库所描述的完整本地 Transformer 推理程序，不依赖云端服务。

## 与作者数据的关系

作者 README 声明约 9.72 compute token/s、约 9.5 token/s 端到端；本次 ESP-IDF
适配版只有 5.76 compute token/s，主要差异在输出头：本次为 105.68 ms/token，
作者声称为 57.6 ms/token。

测试权重不会明显减少矩阵运算次数，但本实验仍不是作者 Arduino Core 3.3.10
固件的逐二进制复刻，编译器、任务封装和测试权重均不同。因此当前结论是：

- “这块 ES3N28P 能否运行完整程序”：已真机证明可以。
- “作者是否真实在本地执行模型”：源码路径和本次真机结果均支持其可行性。
- “作者的 9.5 token/s 是否可复现”：本次没有复现，仍需作者原始模型、
  `vocab.h` 和其精确构建产物或日志。
- “作者演示的故事质量是否真实”：未验证，必须取得原始训练权重或自行训练。

## PGOS 恢复

实验后重新构建并烧录 PGOS：

- RAM：118,632 / 327,680 bytes；
- Flash：2,455,213 / 6,553,600 bytes；
- app0 和 app1 均写入当前 PGOS 镜像并通过写入哈希校验；
- SPIFFS 从备份恢复，SHA-256 为
  `E87FF9875359402D0782BE2D0CCC6A9BCC6D7BB48D01A6F233C004E481CDF022`；
- NVS 恢复前后 SHA-256 均为
  `88A2F197FCF70E240E55C8590099F47FEED4E18AF6329A9BCAF256EC9423EEF9`。

PGOS 启动日志确认 16MB Flash、8MB OPI PSRAM、ILI9341、I2C、RTC、音频、RGB、
Wi-Fi、游戏存档和 USB `status` 均正常恢复。密码和本地网络配置未写入实验记录。
