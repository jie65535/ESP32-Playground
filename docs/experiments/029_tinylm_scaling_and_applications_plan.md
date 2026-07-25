# TinyLM 无 OTA 扩容与应用场景评估

> 日期：2026-07-26
> 状态：方案分析完成，尚未修改分区、训练新模型或烧录
> 前置：[027_chinese_tinylm_assets_and_model.md](027_chinese_tinylm_assets_and_model.md)、[028_pgos_tinylm_integration.md](028_pgos_tinylm_integration.md)
> 训练环境：`C:\Users\Admin\AppData\Local\Programs\Python\Python310\python.exe`，PyTorch 2.13.0+cu130，RTX 3080

## 目标与结论

在用户暂时不需要 OTA 的前提下，评估 PGOS 能给 TinyLM 留出多大模型分区、哪些
参数真正改善生成质量，以及除故事续写以外还能形成哪些本地应用。本记录只保存
计算和下一步实验设计，不把候选配置写成已经验证的硬件结论。

结论先行：

- 保留当前 4MiB PGOS App 和 3.875MiB SPIFFS，仅回收第二个 OTA 槽，可把
  `model` 从 4MiB 扩到 8MiB。
- 沿用当前 `V=8192, D=128, L=6, H=4, F=415, S=512` 时，8MiB 分区的数学
  极限约为 `P=268`、16.19M 参数，但只剩 20KiB，不适合作为交付配置。
- `P=256` 为更合理的容量上限：15.57M 参数，导出 8,041,156 bytes
  （7.67MiB），分区还剩 347,452 bytes。
- 如果目标是质量而不是参数量标题，优先候选应是 `P=192, F=512`：约 12.50M
  参数、6.16MiB 模型，同时保留当前 FFN 并适度增强 dense core。
- 当前模型只采样训练了 65,536,000 tokens，而 `train_zh.bin` 有 112,698,516
  tokens，验证损失到最后仍在下降；先延长训练可能比立即扩容更划算。

## 当前基线

| 项目 | 当前实测/已验证值 |
|---|---:|
| 配置 | V=8192, D=128, L=6, H=4, F=415, P=64, S=512, G=128 |
| 总参数量 | 5,693,632 |
| INT4 `model.bin` | 2,947,012 bytes（2.81MiB） |
| 生成动态 PSRAM | 4,274,108 bytes（约 4.08MiB） |
| 生成中 PSRAM 空闲 | 3,668,588 bytes（约 3.50MiB） |
| PGOS 端到端吞吐 | 5.3--6.0 tok/s |
| 训练量 | 4,000 steps，65,536,000 sampled tokens |
| 训练数据 | 112,698,516 train tokens + 566,324 val tokens |
| 最终验证指标 | loss 2.5911，perplexity 13.34，末段仍缓慢下降 |

## 不做 OTA 时的 Flash 上限

当前分区为两个 4MiB App 槽、4MiB model、3.875MiB SPIFFS 和 64KiB coredump。
如果后续确定不需要 OTA，可改成单个 factory App。以下按当前 INT4 group-128
导出布局和固定 `F=415` 精确计算：

| 分区取舍 | 可给 model 的容量 | 当前参数族可容纳的数学极限 | 模型大小 | 评价 |
|---|---:|---:|---:|---|
| 保留 4MiB App 和 SPIFFS，只回收 app1 | 8MiB | P=268，16,190,860 参数 | 8,368,148 bytes | 仅余 20,460 bytes，不留工程余量 |
| App 缩到 3MiB，保留 SPIFFS | 9MiB | P=308，18,249,140 参数 | 9,431,156 bytes | App 只剩约 380KiB 余量，不利于 PGOS 演进 |
| 保留 4MiB App，移除 SPIFFS | 11.875MiB | P=422，24,115,238 参数 | 12,450,796 bytes | 失去文件系统，PLE 行宽已超过经验甜点区 |
| App 缩到 3MiB，并移除 SPIFFS | 12.875MiB | P=461，26,122,061 参数 | 13,488,432 bytes | 绝对容量方案，不推荐作为日常 PGOS 布局 |

上游英文 28.9M 模型的导出文件为 14,912,332 bytes。它能在独立 619KiB 应用加
近 15MiB 模型分区的固件里运行，但无法和当前约 2.64MiB 的完整 PGOS App 一起
塞进 16MiB Flash。若坚持该量级，需要裁剪 PGOS、外接存储或更大 Flash 芯片。

数值最大也不等于效果最好。上游消融中，增加 PLE 行宽的收益在 `P=256` 左右
达到高点，继续增加到 P512 反而略有回落；因此 P422/P461 主要增加“驻留参数量”，
不应预期同比例增加语言能力。

## 候选配置与资源预测

模型大小按当前 `export.py` 张量顺序和 ragged INT4 + fp16 scale 格式精确计算；
PSRAM 空闲以本轮 7,942,696-byte 空闲基线和当前分配器开销估算，必须再由真机
采样确认。

| 方案 | 配置变化 | 参数量 | 模型 | 预计生成中 PSRAM 空闲 | 取舍 |
|---|---|---:|---:|---:|---|
| 当前基线 | P64, F415 | 5.69M | 2.81MiB | 3.50MiB（实测） | 已验证、速度最好 |
| Flash 扩容 | P192, F415 | 12.28M | 6.05MiB | 3.49MiB | PLE 容量增加，运行内存几乎不变 |
| 推荐平衡 | P192, F512 | 12.50M | 6.16MiB | 3.49MiB | PLE 与 dense FFN 同时增加，优先测试 |
| 合理容量上限 | P256, F415 | 15.57M | 7.67MiB | 3.49MiB | 接近 8MiB 分区上限，预计 PLE 计算更慢 |
| 大词表实验 | V16384, P112, F415 | 14.72M | 7.27MiB | 2.43MiB | 更多词表行，但输出头和 tokenizer 成本明显增加 |
| dense quality | D160, P192, F512 | 13.39M | 6.63MiB | 2.49MiB | 核心能力更可能提升，但注意力、FFN 和 head 都更慢 |
| 增加深度 | L8, P128, F415 | 11.63M | 5.73MiB | 2.49MiB | 深度增加，KV cache 与每 token 计算约同步增加 |

### 参数含义与优先级

- `ple_dim`：最适合消费新增 Flash。PLE table 留在 mmap Flash，P64 → P256
  几乎不增加 KV cache 和输出头，只增加很小的临时缓冲；代价主要是 PLE 投影计算。
- `ffn_hidden`：更直接增加 dense core 容量，通常比只堆 PLE 参数更可能改善语法、
  状态转换和局部推理，但 FFN 当前已经占约 54.7ms/token，扩大后会直接降速。
- `d_model`：同时扩大注意力、FFN、输出头和 KV cache，质量潜力更大，但内存和
  计算代价最全面；D160 可作为偏质量实验，不能直接视为免费升级。
- `n_layers`：增加深度可能改善组合能力，但 KV cache 与层数线性增加；L8 预计比
  当前多使用约 1MiB PSRAM，并增加约三分之一的逐层计算。
- `vocab`：V16384 可能让中文短语 token 化更紧凑，并增加 PLE table 的“行数”；
  但输出头从 8192 行翻倍，会多占约 1.06MiB PSRAM并显著增加每 token head 时间。
- `seq_len`：只增加上下文容量，不增加模型知识或核心能力。S512 已覆盖当前最多
  200-token 输出；S768 会额外消耗约 1.5MiB KV cache，不列为近期优化项。
- `n_heads`：当前 D128/H4 的 head dim 为 32，先保持不变，避免引入无明确收益的
  结构变量。

### `fixed-ffn` 陷阱

`train.py` 默认用 `target-core=1,500,000` 求解 FFN。如果只增大 `ple_dim` 而不传
`--fixed-ffn`，脚本会用更小的 FFN 给 PLE adapter 腾出核心预算：

| P | 默认求解出的 F |
|---:|---:|
| 64 | 415 |
| 128 | 351 |
| 192 | 287 |
| 256 | 223 |
| 268 | 211 |

这仍保持约 1.5M core 参数，却改变了核心内部的能力分配，不能视为“只增加 Flash
表”。扩容实验若要保留当前 FFN，必须显式使用 `--fixed-ffn 415`；推荐平衡方案
则使用 `--fixed-ffn 512`，允许 dense core 一并增长。

## 质量改善的合理预期

提高存储参数量大概率能改善局部连贯性、词语搭配、人物/物体关联和重复问题，但
不会把 TinyStories 模型变成通用助手。PLE 的新增参数是按 token 稀疏读取的记忆表，
它对词汇和每层状态注入有帮助，却不等价于同比扩大 dense reasoning core。

上游可复现结论提供了方向而不是中文模型的保证：

- PLE 行宽从 P64 增至 P256 时，隔离出的收益明显增加；P512 未继续增长，提示行宽
  存在饱和点。
- 增大 vocabulary 的“行数”在测试范围内仍有收益，但输出头变大后会消耗 PSRAM
  带宽和时间。
- 同等实验里扩大 dense core 的收益大于小词表下只加 PLE，因此如果目标是实际
  语言质量，应同时比较 F512、D160 或 L8，而不是只按总参数量排序。
- 任何候选都必须比较 fp32 validation、INT4 validation、固定提示词样本和真机
  tok/s；不能只看训练 loss 或导出尺寸。

无需训练也有一个低成本方向：当前采样仅使用 temperature=0.8 的全词表 softmax。
增加 top-k/top-p 和轻量 repetition penalty，可能先减少重复、跳题和低概率怪词，
应与模型扩容分开做单变量实验。

## 推荐训练顺序

当前训练从 4,000 steps 结束时 validation 仍在下降，而且只采样了相当于训练 bin
约 0.58 倍的 token 数。推荐按以下顺序，而不是直接训练最大模型：

1. 保持当前 P64/F415，训练到 8,000 或 12,000 steps，确认“只增加训练量”的收益。
2. 训练 P192/F512，建议 12,000--16,000 steps，与长训基线使用同一验证集和 prompts。
3. 若 P192 在 INT4 和真机输出上有稳定收益，再训练 P256/F415 或 P256/F512。
4. 最后才考虑 V16384；它需要重新训练 tokenizer、重新编码全部数据、生成新
   `vocab.h` 和提示词 token ID，实验成本和运行时变化都更大。
5. 每个胜出候选至少补一个 seed，避免把随机波动当作结构收益。

推荐平衡配置的从头训练命令：

```powershell
cd G:\MCU\ESP32Playground\src\third_party\esp32_ai
& "C:\Users\Admin\AppData\Local\Programs\Python\Python310\python.exe" `
  src\train.py `
  --arm ple `
  --data-suffix _zh `
  --vocab 8192 `
  --d-model 128 `
  --n-layers 6 `
  --n-heads 4 `
  --ple-dim 192 `
  --fixed-ffn 512 `
  --seq-len 512 `
  --steps 12000 `
  --batch-size 32 `
  --lr 8e-4 `
  --warmup 500 `
  --tag zh-p192-f512
```

当前 `train.py` 没有 checkpoint resume 参数，上述命令是从头训练；若决定延续旧
checkpoint，应先单独实现并测试 resume/fine-tune 流程，不能把重新起跑误记成续训。

## 除故事以外的应用场景

当前权重只学习 TinyStories-Zh，因此现成模型仍以故事续写为主。以下场景需要混合
或替换训练数据，但都符合短上下文、短输出、本地离线和单前台 App 的 PGOS 约束：

- 游戏 NPC 短对话、关卡旁白、胜负评论和角色台词。
- 动态生成道具名、成就描述、任务文本、每日挑战和卡牌事件。
- 桌面电子宠物的问候、情绪文本和两三轮短对话。
- 儿童句子续写、词语联想、填空、简单谜语和短诗。
- 根据 RTC、Wi-Fi、游戏成绩、RGB/音频状态等结构化字段生成自然语言摘要。
- 通过 PGOS Studio 输入开头，做自由续写、模型参数对比和离线隐私演示。
- 为 Blackjack、Platformer、Breakout 等现有游戏增加非关键路径的趣味解说。

不应把该模型用于通用知识问答、可靠算术、代码生成、长对话或安全相关硬件控制。
确定性的设备命令仍由 Console/Service API 和规则解析负责；TinyLM 可以修饰结果或
生成非关键文本，但不能决定功放、电源、Flash 写入等动作。

如果改做对话、命令解释或状态总结，需要准备明确的领域语料格式，例如短轮次
`用户：...\n助手：...` 或结构化状态 → 文本对；当前 ByteLevel tokenizer 虽能无损
表示任意中文，但故事 tokenizer 对新领域未必高效。真正跨领域时应比较“沿用 8192
词表微调”和“用混合语料重训 tokenizer”两条路线。

## 后续集成检查表

决定训练新模型后仍需逐项完成：

- 将分区从双 OTA 迁移为单 App + 目标大小 model，并再次备份 NVS/SPIFFS。
- 更新 `TinyLmService` 的 `MODEL_BYTES`、模型 header 校验和页面模型信息。
- D 改变时确认 `headActivation[256]` 上限；V 改变时重新生成 `vocab.h`、提示词 ID
  和 token 最大字节长度测试。
- 每个模型先跑 C/PyTorch golden 和 INT4 validation，再构建 PGOS。
- 真机分别记录列表空闲、加载、生成中、生成完成未退出、退出后的 internal heap、
  minimum heap、PSRAM、minimum PSRAM、任务 stack high-water mark 和 tok/s。
- 比较固定 prompts、随机多轮样本、重复率、EOT 长度和人工主观连贯性。
- 新分区和新模型确认稳定前保留 028 的备份与当前可回退固件。

本轮只完成评估并记录，模型扩容、取消 OTA、训练参数和新应用方向留待下一次决定。
