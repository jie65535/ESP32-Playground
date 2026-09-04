# 第三方依赖与源码策略

## 当前规则

根目录 `LICENSE` 只覆盖 PlaygroundOS 自有代码和文档。第三方源码、字体、图标、
模型/数据以及 Platformer 的 SMB 派生资源均保留各自的版权和许可证边界；发布前
请同时阅读 [`THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md)。

大型构建依赖不直接进入 PlaygroundOS 的 Git 历史；少量需要固定实现、审查或
离线复现的第三方源码可以按明确提交 vendoring，并在对应目录保留上游来源、
版权、许可证和本地修改说明。根目录 `.gitignore` 忽略：

- `components/`：Arduino、Bluepad32、BTstack 和 LVGL 等本地构建组件；
- `managed_components/`：ESP-IDF Component Manager 的下载结果；
- `.pio/`：PlatformIO 的构建产物。

Git 只保存版本锁定、构建清单和项目自己的适配层。这样不会把数百 MB 的上游源码复制进仓库，也不会把带有本地修改的 nested Git 仓库伪装成干净 submodule。

PGOS Shell 卡片图标来自 `lucide-static 0.468.0`。仓库只保存由
`tools/generate_ui_icons.py` 生成的 15 个 24×24 A8 遮罩，不保存完整字体；
生成脚本固定上游版本，许可证见 `licenses/Lucide-ISC.txt`。

2048 的隐藏自动走棋只借鉴 [`nneonneo/2048-ai`](https://github.com/nneonneo/2048-ai)
（上游提交 `41e298f`）的
Expectimax 节点划分和启发式评估思路；该项目按 MIT License 发布。PGOS 没有
vendoring 上游源码、65536 项行表、转置缓存或平台适配代码，固件中是独立的
固定内存实现。

中文 TinyLM 的训练、导出和推理源码基于 [`slvDev/esp32-ai`](https://github.com/slvDev/esp32-ai) commit
`9c4a214bdef2f9779bdc927b582d4061dd1ae362`。作者随后在 commit
`74744182329f08d7a1badc97e47576ef527532a0` 中仅新增 MIT License，未改变该
源码快照。项目副本位于 `src/third_party/esp32_ai/`，并保留上游 `LICENSE` 和
`UPSTREAM.md`；中文数据准备、资产生成、训练配置、验证工具、生成词表和实验
记录包含 PGOS 本地修改。MIT 只覆盖上游软件，不自动覆盖训练数据、模型权重或
其它第三方依赖；当前 PGOS 模型为本地训练产物，上游未发布的权重没有进入仓库。

麦克风频域降噪固定导入 [`xiph/speexdsp`](https://github.com/xiph/speexdsp) commit
`7a158783df74efe7c2d1c6ee8363c1e695c71226` 的 BSD 源码子集。仓库只保留
preprocessor、固定点 KISS FFT 及其可选 echo-state 接口所需文件，并保留上游
`LICENSE`、`AUTHORS` 和来源说明；ESP32 适配只把 Speex 工作区分配到 PSRAM。

Platformer 的 `src/games/Platformer*` 生成数据和像素数组来自
[Super-Mario-Bros](https://github.com/Gold872/Super-Mario-Bros) 和
[Mario-Level-1](https://github.com/justinmeister/Mario-Level-1) 粉丝工程。参考工程
的源码许可证不等于 Mario/Nintendo 名称、商标和游戏美术的授权；这些文件不属于根目录
MIT，重新分发前必须取得授权或替换为明确许可的原创资源。

## 已落地的锁定方式

`main/idf_component.yml` 声明官方 ILI9341 驱动 `espressif/esp_lcd_ili9341 == 2.0.2`；根目录 `dependencies.lock` 锁定该组件及其 `cmake_utilities` 传递依赖。第一次构建时，组件管理器把源码恢复到被忽略的 `managed_components/`。

`dependencies/components.lock.json` 记录其余本地组件的上游仓库、tag/commit、当前用途和恢复状态。Arduino、Bluepad32、BTstack 和 LVGL 的大型源码树不进入 Git；本地差异保存在 `dependencies/patches/`，小型 IDF manifest 和 Bluepad32 Arduino adapter 保存在 `dependencies/compat/`。

干净克隆后先运行 `python tools/bootstrap_components.py`，它会按锁文件恢复 Arduino、LVGL、Bluepad32 和 BTstack，并应用 `dependencies/patches/` 中的本地兼容补丁；Bluepad32 Arduino adapter 从 `dependencies/compat/` 复制。CI 在 PlatformIO 构建前执行同一个脚本。已有本地组件目录时脚本默认拒绝覆盖，确认需要重新恢复时再加 `--force`。

可以用 `powershell -ExecutionPolicy Bypass -File tools/verify_components.ps1` 检查当前工作区的版本标记；在缺少官方 managed component 的新工作区使用 `-RestoreManaged`，脚本会让 PlatformIO 按 `main/idf_component.yml` 恢复它。

## 为什么暂时不使用 submodule

submodule 只解决“指向某个提交”，不能解决以下问题：

- 本项目需要从上游仓库中抽取子目录作为 IDF component；
- Arduino 和 Bluepad32 有本地构建补丁；
- Bluepad32 Arduino adapter 和 BTstack 不是同一个标准 component 仓库；
- submodule 工作树一旦应用补丁就会变成 dirty，升级和审查反而更困难。

等本地兼容差异全部变成 `patches/components/<name>/*.patch` 后，如果希望使用 Git 原生工作流，可以再把无修改的上游快照换成 submodule；这不是当前阶段的必要条件。

## 后续清理顺序

1. 保留 `dependencies/components.lock.json` 的精确提交；
2. 从上游快照生成小型、可审查的 patch 文件；
3. 写幂等的 bootstrap 脚本：下载/校验提交、抽取 component 子目录、应用 patch；
4. 在一个空工作区做完整构建验证；
5. 原生 `esp_lcd` 后端稳定后继续提取 Arduino/Bluepad32 的小型 patch；BitmapFont 保留为与显示传输无关的 RGB565 画布资产。
