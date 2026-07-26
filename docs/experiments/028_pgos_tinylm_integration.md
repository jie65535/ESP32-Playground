# PGOS 中文 TinyLM 集成与真机验收

> 日期：2026-07-26
> 状态：集成、分区迁移和实体交互验收通过；7.56M 参数模型已升级、烧录并完成双轮真机冒烟，长期稳定性继续观察
> 前置：[027_chinese_tinylm_assets_and_model.md](027_chinese_tinylm_assets_and_model.md)
> 硬件：QD ES3N28P，ESP32-S3 R8N16，16MB Flash，8MB OPI PSRAM

## 目标

把 027 已在独立固件上跑通的中文 TinyLM 作为 PGOS 前台 App 集成：System Tools
进入 `TinyLM`，选择预设开头后立即生成，文字以 token 流式出现；A 重新生成当前
开头，B 从生成页返回列表，再按 B 返回 System Tools。推理不能阻塞 LVGL、USB、
Wi-Fi、音频和系统输入。

## 资产收拢与忽略策略

- 上游工作副本：`src/third_party/esp32_ai/`。
- TinyStories-Zh-1M 两个原始 Parquet 分片放在
  `src/third_party/esp32_ai/data/raw/TinyStories-Zh-1M/`，合计约 430MB。
- `prepare_zh.py` 默认读取上述本地 raw 目录；此前 Qwen/Ollama 生成故事的遗留脚本
  和素材没有带入当前数据目录。
- raw Parquet、tokenizer/training bin、checkpoint 和 `model.bin` 由嵌套
  `.gitignore` 排除；设备需要的 `vocab.h` 和生成后的精简字体进入源码资产。
- `tools/generate_tinylm_font.py` 从原始数据集 `text` 字段提取全部可打印、非空白、
  去重字符，再从 Fusion Pixel BDF 生成 `src/ui/TinyLmFontData.h`。

字体扫描共处理 1,016,991 篇故事，数据集有 3,970 个非空白字符；BDF 覆盖 3,957
个数据集字符，加入 ASCII/替换字形后最终生成 3,964 个 glyph。12 个极少见且 BDF
不支持的字符在运行时显示为 U+FFFD，不会静默留白。编译后字体数据约 186KiB。

## 模型与运行时预算

| 项目 | 当前值 |
|---|---:|
| 配置 | V=8192, D=128, L=6, H=4, F=512, P=96, S=512, G=128 |
| core 参数 | 1,796,576 |
| tied embedding/head 参数 | 1,048,576 |
| PLE table 参数 | 4,718,592 |
| 总参数量 | 7,563,744（7.56M） |
| `model.bin` | 3,917,660 bytes，SHA-256 `7CE7E90D3D738B94FD169B37FBCA7998F169C635D5C4FD9DDD86A4A9CB2CA069` |
| int8 输出头 + scale | 1,081,344 bytes |
| KV/scratch/logits | 3,196,160 bytes |
| 推理期 PSRAM 动态预算 | 4,277,504 bytes，约 4.08MiB |

`TinyLmService` 启动时只查找、校验并 mmap `model` 分区，不立即分配 4MB PSRAM。
进入生成页后再展开 int8 输出头并分配 KV/scratch；退出 TinyLM App 后异步释放。
推理任务固定在 core 1，输出头协作任务固定在 core 0，服务用深度 1 的控制队列和
深度 64 的 token 队列交付数据。取消在 token 边界生效，不从工作任务直接操作 LVGL。
推理源文件单独使用 `-O3`，PGOS 其它模块保持原构建优化设置。

当前 UI 每 120ms 合并一次 token 文本更新；ByteLevel BPE token 可能在 UTF-8 多字节
字符中间断开，因此 App 保存增量解码状态，只有形成完整 codepoint 后才写入 LVGL。
当前单次最多生成 200 token，提示词占 3 token，模型上下文上限为 512 token；遇到
EOT token 0 会提前结束。A 会取消旧 generation 并启动新 generation，旧队列事件由
generation ID 丢弃。

## Flash 分区迁移

| 名称 | Offset | Size | 用途 |
|---|---:|---:|---|
| nvs | 0x9000 | 0x5000 | 设置、Wi-Fi、成绩 |
| otadata | 0xE000 | 0x2000 | OTA 选择状态 |
| app0 | 0x10000 | 0x400000 | PGOS OTA slot 0 |
| app1 | 0x410000 | 0x400000 | PGOS OTA slot 1 |
| model | 0x810000 | 0x400000 | TinyLM INT4 模型 |
| spiffs | 0xC10000 | 0x3E0000 | PGOS 文件系统 |
| coredump | 0xFF0000 | 0x10000 | 崩溃转储 |

相较旧 PGOS，两个 app 槽从 6.25MB 收缩为 4MB，并在中间加入 4MB 模型分区；
SPIFFS 从 3.375MB 调整为 3.875MB 且 offset 改变。因此迁移前必须分别备份 NVS、
otadata、旧 SPIFFS 和 coredump，不能只上传新应用覆盖旧分区表。模型与 OTA 应用
镜像分离，日后常规 OTA 不必重复传 3.92MB 模型。

## 软件验证

```powershell
pio run -e playground
python -m unittest discover -s tools/tests -p "test_*.py"
gcc -O3 src\third_party\esp32_ai\firmware\host_verify\verify.c -lm `
  -o .pio\host_verify\tinylm_verify.exe
.pio\host_verify\tinylm_verify.exe `
  src\third_party\esp32_ai\firmware\model\model.bin `
  src\third_party\esp32_ai\firmware\model\golden.txt
```

结果：

- 升级后重新构建通过：RAM 118,880 / 327,680 bytes（36.3%）；Flash
  2,766,273 / 4,194,304 bytes（66.0%），`firmware.bin` 为 2,766,672 bytes。
- 64 项主机测试通过；新增测试锁定字体、8192 词表、64-byte token 解码缓冲、
  model 大小和分区无重叠。
- C 推理与 PyTorch golden 的 top token 均为 271，max abs diff 0.00002，
  RMS diff 0.000003，数值验证 PASS。

## 资源占用汇总

以下数据同时区分构建期静态占用、Flash 分区占用和真机运行时 PSRAM，避免把
`firmware.bin`、模型文件大小和动态推理工作区混为一谈：

| 资源 | 当前占用或空闲 | 分区/总量 | 余量与结论 |
|---|---:|---:|---|
| 内部 SRAM 静态占用 | 118,880 bytes | 327,680 bytes | 占 36.3%，链接期余量 208,800 bytes（约 203.9KiB） |
| 单个 OTA App | `firmware.bin` 2,766,672 bytes | 4,194,304 bytes | 占约 66.0%，每个槽余量 1,427,632 bytes（约 1.36MiB） |
| TinyLM 模型 | 3,917,660 bytes | 4,194,304 bytes | 占 93.4%，模型分区余量 276,644 bytes（约 270.2KiB） |
| TinyLM 字体 | 190,272 bytes | 编入 App Flash | 3,964 glyph，约 185.8KiB |
| TinyLM 词表 | 92,755 bytes | 编入 App Flash | offset 表 32,772 bytes，字符串数据 59,983 bytes，合计约 90.6KiB |

### 真机运行阶段对比

运行时资源必须按阶段记录。单独写一个“TinyLM 占用 4.08MiB”会掩盖它是生成时
才按需申请的峰值，也无法判断退出后是否泄漏。本轮已有证据如下：

| 阶段 | 行为 | PSRAM 空闲 | 相对标称 8MiB 的非空闲量 | TinyLM 动态增量 | 结论 |
|---|---|---:|---:|---:|---|
| 空闲基线 | 已进入 TinyLM 预设列表，尚未生成 | 7,947,296 bytes（约 7.58MiB） | 441,312 bytes（约 431.0KiB） | 0 | 模型仅 mmap，未申请推理工作区 |
| 生成中 | 输出 token、更新流式页面 | 3,669,732 bytes（约 3.50MiB） | 4,718,876 bytes（约 4.50MiB） | 4,277,564 bytes（约 4.08MiB） | 与 4,277,504-byte 静态预算只差分配器开销，仍有约 3.50MiB 空闲 |
| 生成完成、仍停留页面 | 输出已停止，页面等待 A/B | 3,669,732 bytes（约 3.50MiB） | 4,718,876 bytes（约 4.50MiB） | 4,277,564 bytes（约 4.08MiB） | 工作区按设计保留，可立即重新生成 |
| 退出 App 两秒后 | 返回 System Tools | 7,947,296 bytes（约 7.58MiB） | 441,312 bytes（约 431.0KiB） | 0 | 精确恢复到本轮空闲基线，未观察到泄漏 |

上表只把有真机证据的运行时 PSRAM 数值写成实测。内部 SRAM 的
118,880 / 327,680 bytes 是链接期静态占用，不等于运行时 free heap；本轮状态快照中
预设页、生成中、第二轮完成和退出后的 free heap 分别为 54,036、53,588、53,288 和
55,832 bytes，但采样时 BLE 扫描阶段不同，不能把差值全部归因于 TinyLM。后续长测
仍应补采 internal minimum free heap、PSRAM minimum free 和两个 TinyLM 任务的
stack high-water mark。

按升级后构建计算，两份 OTA App 二进制加模型的核心 Flash 载荷为 9,451,004 bytes
（约 9.01MiB）；此外仍保留 3.875MiB SPIFFS，以及 NVS、OTA data 和 coredump。
TinyLM 的 3.92MB INT4 权重直接从 Flash mmap，不会再完整复制到 PSRAM；4.08MiB
动态增量主要由 1,081,344 bytes 的 int8 输出头与 scale，以及 3,196,160 bytes 的
KV/scratch/logits 构成。

当前资源结论是：内部 SRAM、App 分区和运行时 PSRAM 都没有贴线，生成过程中仍有
约 3.50MiB PSRAM 可用，退出后能完整回收。若以后扩大模型，当前首先可见的硬边界
是模型分区仅余约 270KiB；同时应给 Wi-Fi、LVGL、音频和镜像保留至少 1.5--2MiB
运行时 PSRAM 安全余量。当前 7.56M 模型仍保留约 3.50MiB 生成中 PSRAM，短测吞吐
约 5.1--5.2 tok/s；继续扩张前应先比较实际文本收益，而不是把 4MiB 分区完全填满。

## 烧录与迁移记录

COM3 确认为 Espressif USB-Serial/JTAG（VID:PID `303A:1001`），芯片为 ESP32-S3
rev 0.2、16MB quad Flash、8MB embedded OPI PSRAM。迁移前备份位于被 Git 忽略的：

```text
artifacts/tinylm-pgos-migration/20260726-011851/
```

关键备份：

| 文件 | 大小 | SHA-256 |
|---|---:|---|
| `nvs.bin` | 20,480 | `88A2F197FCF70E240E55C8590099F47FEED4E18AF6329A9BCAF256EC9423EEF9` |
| `old-spiffs.bin` | 3,538,944 | `E87FF9875359402D0782BE2D0CCC6A9BCC6D7BB48D01A6F233C004E481CDF022` |
| `partition-table.bin` | 4,096 | `645A36C0915CEC35CBCB6CF1A6087D70040E53119E69BAD7960D5E48F7D417EA` |
| `standalone-factory-app.bin` | 1,048,576 | `F7F0D481A5FB794329FFBD0B3BA41242B7856C769C3C453091869CFACD13841A` |
| `coredump.bin` | 65,536 | `208892BC11498D197D49F6D317B74C765AAED786ABF2E3AB2AF72595B918F600` |

当前 NVS 和旧 SPIFFS 的哈希与 026 恢复备份逐字节一致。旧分区表回读确认是
`factory 1MB + model 14,875MB + coredump 64KB`。由于 USB/JTAG 使用 stub 在读取
Flash 时出现过一次 `Packet content transfer stopped`，最终备份统一改用
`--no-stub`；每个文件只有达到预期长度才被接受。

新 `model` 和 `spiffs` 精确区域擦除后，写入 bootloader、分区表、OTA data、两份
相同 PGOS 应用和模型：

```powershell
python -m esptool --port COM3 --baud 460800 erase-region 0x810000 0x400000
python -m esptool --port COM3 --baud 460800 erase-region 0xC10000 0x3E0000
python -m esptool --chip esp32s3 --port COM3 --baud 460800 write-flash `
  --flash-mode dio --flash-freq 80m --flash-size 16MB `
  0x0      .pio\build\playground\bootloader.bin `
  0x8000   .pio\build\playground\partitions.bin `
  0xE000   .pio\build\playground\ota_data_initial.bin `
  0x10000  .pio\build\playground\firmware.bin `
  0x410000 .pio\build\playground\firmware.bin `
  0x810000 src\third_party\esp32_ai\firmware\model\model.bin
```

两份应用和模型的写入哈希均由 esptool 验证。`firmware.bin` 为 2,766,656 bytes，
SHA-256 `D4D80C238845E2817D8F872CE75123D3E8AA134E1DA04F8937AE61F497F40AE9`。
烧录后回读分区表，offset/size 与 `partitions_16MB.csv` 完全一致。

### 7.56M 模型原位升级

2026-07-26 在不修改 4MiB 模型分区和 OTA 布局的前提下，将模型从 P64/F415
升级为 P96/F512。旧 `model.bin` 备份位于：

```text
artifacts/tinylm-p96-f512-12k/20260726-144050/old-model.bin
```

旧模型 SHA-256 为
`A4C4F8211ABBCFFA689F08D45B5FC5A9EF0E7AA93937F36CA90286A593C09566`。
新模型和本轮固件如下：

| 文件 | 大小 | SHA-256 |
|---|---:|---|
| `firmware.bin` | 2,766,672 | `7CD467A27155D6D6B85F439D1B0F57EE1669B9DD71D44ADBF901DA6A69FE4EB3` |
| `model.bin` | 3,917,660 | `7CE7E90D3D738B94FD169B37FBCA7998F169C635D5C4FD9DDD86A4A9CB2CA069` |

PlatformIO 上传更新 bootloader、分区表、OTA data 和 app0；随后把同一份
`firmware.bin` 单独写入 `0x410000` 的 app1，避免未来切换 OTA 槽时旧固件因
F/P header 不匹配而拒绝新模型。新模型写入 `0x810000`。app0、app1 和 model
三次写入均由 esptool 完成哈希校验，未擦除 NVS 或 SPIFFS。

## 自动化真机冒烟结果

### 初始 5.69M 模型

PGOS 启动后旧设置仍可用：Wi-Fi 自动连接、RTC/NTP、LCD DMA、音频、RGB、麦克风
和手柄服务均报告 ready/正常状态。NVS 在启动后因系统服务的正常运行时写入而发生
变化，但 Wi-Fi、音量、RGB、麦克风等用户配置均由状态查询确认保留。

通过 USB 统一输入实际执行 Desktop → System Tools → TinyLM：

- 预设列表标题、副标题、四个故事开头和焦点卡片显示正常；第四项可滚动进入。
- 第一次生成在 32/200 token 时为 5.8 tok/s；中文完整，无 UTF-8 半字或空白缺字。
- A 重新生成后故事内容改变，22/200 token 时为 6.0 tok/s。
- 一次完整生成在 EOT 提前结束于 70 token，最终显示 5.3 tok/s 和“完成”。
- 故事区域自动滚动到最新文本；底部 A/B 提示和 token/速度信息可见。
- B 第一次停止/返回预设列表，第二次返回 System Tools；重新进入 App 正常。
- TinyLM 列表冷态 PSRAM 为 7,942,696 bytes；推理中为 3,668,588 bytes，差值
  4,274,108 bytes；退出 App 两秒后恢复到 7,942,696 bytes。
- 生成、重生成、取消、退出和重新进入期间没有复位、abort、WDT 或模型错误。

截图保存在本地 `captures/tinylm-presets.png`、`tinylm-generating.png`、
`tinylm-regenerated.png`、`tinylm-complete.png` 和
`tinylm-ready-for-acceptance.png`。测试期间 Wi-Fi/BT 共存栈偶发两条
`wifi:m f null` warning，但 Wi-Fi 始终保持连接，TinyLM 和其它服务未受影响；长测时
继续观察，不把它归因于模型。

初始验收后设备停在 TinyLM 预设列表，PSRAM 已释放。2026-07-26 用户完成实体测试，
反馈“交互体验还行”，因此预设列表、流式生成和 A/B 导航的主观交互验收记为通过。
本轮短时验收不替代连续多轮生成、Wi-Fi/BT 共存和长时间运行稳定性测试，后续仍需
在日常使用中继续观察。

### P96/F512 7.56M 模型升级

升级后完整重启日志确认实际加载：

```text
[tinylm] model ready V=8192 D=128 L=6 F=512 P=96 S=512 size=3.92 MB
```

通过 USB 统一输入再次执行 Desktop → System Tools → TinyLM，并完成两轮连续生成：

- 预设页显示 `7.56M 参数 · INT4 3.92 MB · 本地运行`，中文、滚动和焦点正常。
- 第一轮在生成中截图时为 69/200 token、5.2 tok/s，最终 EOT 提前结束于 85 token，
  页面显示约 5.1 tok/s。文本形成完整段落，但出现“小兔子/猴子”等对象漂移。
- A/OK 重新生成得到不同内容；第二轮 EOT 提前结束于 77 token，日志为
  `compute=5.09 tok/s`。文本仍能形成完整段落，但局部因果和角色一致性尚不稳定。
- 两轮均无 UTF-8 半字、空白缺字、模型错误、abort、WDT 或异常复位。
- 预设页、生成中/完成和退出两秒后的 PSRAM 分别为 7,947,296、3,669,732 和
  7,947,296 bytes；动态增量 4,277,564 bytes，退出后精确回收。
- B 第一次返回预设列表，第二次返回 System Tools；重新进入 TinyLM 正常。
- Wi-Fi 本轮按设备设置处于 off；BLE 手柄扫描/重连日志持续运行，未阻塞生成。

本轮截图保存在本地：

```text
captures/playground-20260726-145859.png
captures/playground-20260726-145938.png
captures/playground-20260726-150046.png
captures/playground-20260726-150139.png
captures/playground-20260726-150214.png
```

设备在双槽同步和重启复核后再次进入 TinyLM 预设列表。短测说明升级模型可以在原
4MiB 分区和现有约 4.08MiB 推理工作区内稳定运行，端侧速度只比初始模型略低；文本
局部连贯性有所改善，但两轮样例仍暴露角色、物体和因果关系漂移，不能仅凭 validation
PPL 把它视为已解决故事一致性。

不做 OTA 时的 Flash 上限、候选训练配置、质量收益和其它应用场景已单独记录在
[029_tinylm_scaling_and_applications_plan.md](029_tinylm_scaling_and_applications_plan.md)；
029 已记录首轮 P96/F512 扩容结果，更大模型和跨领域数据集仍属于后续评估。
