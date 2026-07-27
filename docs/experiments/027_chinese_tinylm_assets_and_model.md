# ESP32-S3 中文 TinyLM：训练、导出与真机推理

> 日期：2026-07-25
> 状态：完整 pipeline 真机通过；中文故事生成正常
> 前置：[026_esp32_ai_runtime_probe.md](026_esp32_ai_runtime_probe.md)
> 硬件：QD ES3N28P，ESP32-S3 rev 0.2，16MB Flash，8MB OPI PSRAM
> 训练机：RTX 3080 12GB
> PGOS 基线：`1a97eb0`
> 上游：`slvDev/esp32-ai`，提交 `9c4a214`；当前工作副本位于 `src/third_party/esp32_ai/`
> 授权更新（2026-07-27）：上游提交 `7474418` 新增 MIT License；项目副本已保留许可证和来源说明

## 目标

在 026 已确认 ES3N28P 能执行完整 PLE TinyLM 推理链的基础上，从头训练一套
中文 tokenizer 和 PLE 权重，让开发板从 `"从前，有一只小兔子"` 提示词开始
生成中文短故事（200 token），串口以 UTF-8 明文输出。

---

## 一、选型决策

### 1.1 数据集

**选用**：[`RobinChen2001/TinyStories-Zh-1M`](https://huggingface.co/datasets/RobinChen2001/TinyStories-Zh-1M)

本实验下载该数据集提供的 Parquet 文件，在本地读取中文 `text` 字段；文本量覆盖
全部训练需要。此前尝试过用本地 Qwen/Ollama 自行生成中文儿童故事，但最终没有
使用该路线，相关生成脚本、词表素材和少量测试故事已清理。

### 1.2 Tokenizer

调研阶段推荐 `flyingfishinwater/tinystories_zh` 的 SentencePiece tokenizer（V=3000）。
**实际选用 ByteLevel BPE（V=8192）**，原因：

- 上游 `esp32-ai` 的 `gen_assets.py` 和推理代码已经实现了完整的 ByteLevel BPE
  decode 路径，直接复用 `gen_assets_zh.py` 改写比换 tokenizer 库风险更低。
- ByteLevel BPE 对中文的 bytes/token 约 5–6，vocab=8192 在故事质量与模型大小
  之间取得合理平衡。

ByteLevel BPE 中汉字不以 UTF-8 直接出现在 vocab 表里，而以 GPT-2 风格的
Unicode 转义字符（如 `ĠæĿ±`）表示；这一行为属于设计而非错误。

### 1.3 模型配置

调研方案为 `D=96, V=3000`；实际选用更大配置以提升语言质量：

| 参数 | 调研方案 | 实际执行 |
|------|---------|---------|
| V | 3000 | **8192** |
| D | 96 | **128** |
| L | 6 | 6 |
| H | 4 | 4 |
| F | 66 | **415** |
| P | 128 | **64** |
| S | 256 | **512** |
| G | 128 | 128 |
| rope_theta | — | 10000.0 |

---

## 二、数据准备（`data/prepare_zh.py`）

通过 `pyarrow.parquet` 读取从 `RobinChen2001/TinyStories-Zh-1M` 下载到本地的
Parquet 分片，提取 `text` 字段，过滤过短、过长、明显未翻译或带元评论的样本，
再用 `<|endoftext|>` 分隔故事。处理后的训练/验证数据分别写为 uint16 token ID。

**训练切片**：取前 2M 字符（约 6 MB UTF-8），对 ByteLevel BPE 训练足够；
之后全量文本用于生成 `train_zh.bin` / `val_zh.bin`。

### 数字显示 bug（已修复）

原始代码：

```python
print(f"Total text: {len(text)/1e6:.1f} MB")
```

`len(text)` 是 Python 字符数，不是字节数。中文字符每个 3 字节，
结果显示 "231.9 MB" 但实际约是描述字符数（2.32 亿字符）。

修复：

```python
slice_text = text[:TOKENIZER_TRAIN_CHARS]
print(f"Slice: {len(slice_text.encode())/1e6:.1f} MB")
```

### bytes/token

```
Avg bytes/token: 5.84
```

这是 ByteLevel BPE 对中文的正常值：一个 UTF-8 汉字占 3 字节，
vocab=8192 的 BPE 约能将2–3个汉字合并成一个 token，故 5–6 bytes/token 符合预期。

---

## 三、Tokenizer 训练

使用 `tokenizers` 库，ByteLevel BPE，vocab=8192，在 2M 字符切片上训练，
耗时 < 2 秒，输出 `bpe8192_zh.json`。

训练命令（当前目录 `src/third_party/esp32_ai/`）：

```powershell
python src\train.py --arm ple --data-suffix _zh --vocab 8192
```

（`--armple` 无效，正确写法是 `--arm ple`，两个 token）

### ByteLevel BPE byte decoder bug

`gen_assets_zh.py` 初版用 `enumerate(ByteLevel.alphabet())` 建立 unicode→byte 映射：

```python
# 错误写法
_ALPHA = ByteLevel.alphabet()
_BYTE_DEC = {c: i for i, c in enumerate(_ALPHA)}
```

`ByteLevel.alphabet()` 返回按 **Unicode codepoint** 排序的列表，
而非按字节值排序。`enumerate` 给出的 index 与真实字节值不对齐，
导致解码出错，`vocab.h` 里的 UTF-8 字节序列错误，串口输出乱码。

**修复**：重新实现 GPT-2 `bytes_to_unicode()` 映射并取逆：

```python
def _build_byte_decoder() -> dict:
    bs = (
        list(range(ord("!"), ord("~") + 1))    # 33–126 → 自身
        + list(range(ord("¡"), ord("¬") + 1))  # 161–172 → 自身
        + list(range(ord("®"), ord("ÿ") + 1))  # 174–255 → 自身
    )
    cs = list(bs)
    n = 0
    for b in range(256):
        if b not in bs:
            bs.append(b)
            cs.append(256 + n)   # 不可打印字节 → U+0100 起
            n += 1
    return {chr(c): b for b, c in zip(bs, cs)}  # unicode_char → byte_value

_BYTE_DEC = _build_byte_decoder()
```

修复后 roundtrip 验证：

```
"从前，有一只小兔子" → [407, 262, 4316] → "从前，有一只小兔子"  ✓
```

---

## 四、模型训练

```powershell
cd G:\MCU\ESP32Playground\src\third_party\esp32_ai
python src\train.py --arm ple --data-suffix _zh --vocab 8192
```

训练在 RTX 3080 12GB 上运行，约 4000 步完成：

```
ple-s0 step 3999 ... ple-s0 DONE  core=1,499,328  table=3,145,728  val=2.5911  ppl=13.34
```

| 指标 | 值 |
|------|-----|
| 最终 step | 3999 |
| val loss | 2.5911 |
| perplexity | 13.34 |
| core 参数量 | 1,499,328 |
| tied embedding / output head 参数量 | 1,048,576 |
| PLE table 参数量 | 3,145,728 |
| 总参数量 | **5,693,632** |

---

## 五、导出与资源生成

**模型导出**（`src/export.py`）：

```
G:\MCU\ESP32Playground\src\third_party\esp32_ai\firmware\model\model.bin
大小：2,947,012 bytes（2.95 MB）
tensors：65
magic：0x504C4531
量化：int4 weights，fp32/fp16 norms/scales
```

训练 checkpoint `runs/ple-s0.pt` 为 22,794,841 bytes（22.79 MB）；纯 FP32
参数本体为 22,774,528 bytes。设备使用的 `model.bin` 将主要权重量化为 int4，
所以只有 2.95 MB（2.81 MiB）。

**词表生成**（`src/gen_assets_zh.py`）：

```
G:\MCU\ESP32Playground\src\third_party\esp32_ai\firmware\esp32_llm\vocab.h
词表大小：8192 tokens
文件大小：300,011 bytes（约 293 KiB）
```

提示词 token ID：

```cpp
// "从前，有一只小兔子"
static const int PROMPT_IDS[] = {407, 262, 4316};
```

---

## 六、固件工程（`zh_llm` 项目）

固件路径：`artifacts/esp32-ai-validation/work/zh_llm/`

与 `llm_probe` 的主要差异：

- 不做硬编码模型配置校验
- 包含 `vocab.h`（8192 tokens）
- `emit()` 函数直接 `fwrite` UTF-8 字节到 stdout（串口明文输出）
- 提示词：`kPromptIds[] = {407, 262, 4316}`
- `kGenTokens = 200`，`kActiveVocab = VOCAB_N`（8192）

### bug 1：PSRAM free: 0 KB（`sdkconfig.defaults` 缺失）

首次构建后设备崩溃循环，串口输出：

```text
PSRAM free: 0 KB
model V=8192 D=128 L=6 H=4 F=415 P=64 S=512  (15.60 MB)
PSRAM alloc failed (1048576 B)
abort() was called at PC 0x42001acf on core 1
```

这里的 `15.60 MB` 来自 `part->size`，表示 `model` Flash 分区的总容量
`0xEE0000`，不是 `model.bin` 的实际大小；当前模型文件实际为 2.95 MB。

根因：项目根目录只有 `sdkconfig.zh_llm`，缺少 `sdkconfig.defaults`。
PlatformIO ESP-IDF 用 `sdkconfig.defaults` 设置初始配置（包括 PSRAM）；
`sdkconfig.zh_llm` 在新建构建缓存时未被识别为默认来源。

修复：从 `llm_probe` 复制 `sdkconfig.defaults`，包含以下关键项：

```
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_SPIRAM_USE_MALLOC=y
CONFIG_SPIRAM_MALLOC_ALWAYSINTERNAL=4096
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y
```

### bug 2：GCC ICE（`esp_lcd_panel_rgb.c:700`）

清空构建缓存重新构建时，编译中止：

```
C:/.../esp_lcd/rgb/esp_lcd_panel_rgb.c:700:1:
    internal compiler error: Segmentation fault
```

问题函数：`rgb_panel_draw_bitmap`（esp_lcd_panel_rgb.c:603–700），
其中 `COPY_PIXEL_CODE_BLOCK` 宏展开触发 GCC 内部错误。
该错误与优化级别（`-O2` 或 `-O3`）无关，是 GCC 版本 bug。

修复：在框架文件对应函数前插入 pragma，仅对该函数降低优化：

```c
// 文件：C:/Users/Admin/.platformio/packages/framework-espidf/
//       components/esp_lcd/rgb/esp_lcd_panel_rgb.c
#pragma GCC optimize("O1")  // workaround: GCC ICE with COPY_PIXEL_CODE_BLOCK
static esp_err_t rgb_panel_draw_bitmap(...)
```

该文件为 ESP-IDF 框架全局文件（路径在 `.platformio/packages/`），
修改影响同机器上所有使用该 ESP-IDF 版本的 PlatformIO 项目，但只影响
`rgb_panel_draw_bitmap` 一个函数，且该函数在纯 LLM 推理场景中不会被调用。

---

## 七、烧录

**固件**（PlatformIO 上传）：

```
构建大小：297 KB，Flash 占用 1.8%
========================= [SUCCESS] Took 16.52 seconds =========================
```

**模型**（esptool 直接写分区，offset `0x110000`）：

```powershell
python -m esptool --port COM3 --baud 460800 write-flash 0x110000 `
    G:\MCU\ESP32Playground\src\third_party\esp32_ai\firmware\model\model.bin
```

```
Compressed 2947012 bytes to 2532807...
Wrote 2947012 bytes at 0x00110000 in 22.8 seconds
Hash of data verified.
```

分区表：

| 名称 | 类型 | Offset | Size |
|------|------|--------|------|
| nvs | data/nvs | 0x9000 | 0x5000 |
| factory | app/factory | 0x10000 | 0x100000 |
| model | data/0x40 | 0x110000 | 0xEE0000 |
| coredump | data/coredump | 0xFF0000 | 0x10000 |

---

## 八、真机结果

串口监视器（115200 baud，UTF-8），设备启动约 3 秒后开始推理：

```text
=== ESP32-S3 中文 TinyLM (Exp 027) ===
PSRAM free: 8157 KB
model V=8192 D=128 L=6 H=4 F=415 P=64 S=512  (xx MB)
...
了什么，本？"本看着本，说道："我不知道，本。我躲在灌木丛后面，我找不到你。"
本很伤心，但本有一个主意。 "本，我找到你了！"他说。本用他柔软的爪子寻找本。
最后，本找到了本，本非常高兴。 "谢谢你，本！"本说。 "不客气，本。现在我们可以一起玩捉迷藏了！"
本很高兴，他们一起跳得很开心。<|endoftext|>从前，有一个小女孩，名叫莉莉。她喜欢和家人一起去海滩。有一天，
他们决定去海边。他们收拾好行李，开始收拾行李。
```

输出连贯，`<|endoftext|>` 后自动开启新故事，UTF-8 显示正常。
角色名"本"频繁出现是 TinyStories 机器翻译特征（英文 "Ben" 被译为 "本"），
属于数据集固有特点，不影响验证目标。

---

## 九、技术问题汇总

| # | 问题 | 根因 | 修复 |
|---|------|------|------|
| 1 | `prepare_zh.py` 显示 "231.9 MB" 实为字符数 | `len(text)/1e6` 不是字节 | `len(slice_text.encode())/1e6` |
| 2 | `--armple` 命令行参数报错 | argparse 需要 `--arm ple`（两个token） | 改为 `--arm ple` |
| 3 | `gen_assets_zh.py` UnicodeDecodeError | `enumerate(ByteLevel.alphabet())` 给出 Unicode 序而非字节序索引 | 重新实现 GPT-2 `bytes_to_unicode()` 映射取逆 |
| 4 | `PSRAM free: 0 KB`，推理崩溃 | 缺少 `sdkconfig.defaults`，PSRAM 未初始化 | 从 `llm_probe` 复制 `sdkconfig.defaults` |
| 5 | GCC ICE（`esp_lcd_panel_rgb.c:700`） | GCC 版本 bug，`COPY_PIXEL_CODE_BLOCK` 宏展开触发 | 在框架文件函数前加 `#pragma GCC optimize("O1")` |

---

## 十、吞吐量

| 指标 | 026（V≈25K, D=96） | 027（V=8192, D=128） |
|------|-------------------|---------------------|
| 端到端吞吐 | 5.61 tok/s | **7.68 tok/s** |
| compute-only | 5.76 tok/s | **7.80 tok/s** |
| compute ms/token | 173.6 ms | **128.2 ms** |
| input | 6.79 ms | 3.0 ms |
| attention | 37.13 ms | 40.1 ms |
| FFN | 10.60 ms | 54.7 ms |
| PLE | 13.36 ms | 6.0 ms |
| output head | 105.68 ms | **24.4 ms** |

输出头从 105 ms 降至 24 ms（词表缩小4倍）；FFN 增加（D=128, F=415 > D=96, F=66）。

## 十一、Temperature Sampling

初始固件使用 argmax（贪心），每次推理输出固定。
后续添加 temperature softmax 采样（`kTemp = 0.8f`），使用 ESP32-S3 硬件 RNG（`esp_random()`）：

```cpp
// main.cpp — Generate loop（替换 argmax）
float *lp = scratch.logits;
float mx = lp[0];
for (int v = 1; v < VOCAB_N; v++) if (lp[v] > mx) mx = lp[v];
float sum = 0.0f;
for (int v = 0; v < VOCAB_N; v++) { lp[v] = expf((lp[v] - mx) / kTemp); sum += lp[v]; }
float r = (float)(esp_random()) / (float)UINT32_MAX;
float cdf = 0.0f; tok = VOCAB_N - 1;
for (int v = 0; v < VOCAB_N; v++) { cdf += lp[v] / sum; if (r < cdf) { tok = v; break; } }
```

采样开销约 0.1 ms/token，compute 吞吐不变（7.80 tok/s）。

## 十二、后续

- [ ] 尝试将 vocab=8192 的模型质量与调研阶段估算（D=96, V=3000）对比
- [x] 将上游工作副本、原始数据集和当前模型迁入 PGOS 工作区并设置本地忽略
- [x] 集成到 PGOS 的过程转入 [028_pgos_tinylm_integration.md](028_pgos_tinylm_integration.md)
