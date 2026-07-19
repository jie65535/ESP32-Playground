# 第三方依赖与源码策略

## 当前规则

第三方源码不直接进入 PlaygroundOS 的 Git 历史。根目录 `.gitignore` 忽略：

- `components/`：Arduino、Bluepad32、BTstack 和 LVGL 等本地构建组件；
- `managed_components/`：ESP-IDF Component Manager 的下载结果；
- `.pio/`：PlatformIO 的构建产物。

Git 只保存版本锁定、构建清单和项目自己的适配层。这样不会把数百 MB 的上游源码复制进仓库，也不会把带有本地修改的 nested Git 仓库伪装成干净 submodule。

## 已落地的锁定方式

`main/idf_component.yml` 声明官方 ILI9341 驱动 `espressif/esp_lcd_ili9341 == 2.0.2`；根目录 `dependencies.lock` 锁定该组件及其 `cmake_utilities` 传递依赖。第一次构建时，组件管理器把源码恢复到被忽略的 `managed_components/`。

`dependencies/components.lock.json` 记录其余本地组件的上游仓库、tag/commit、当前用途和迁移状态。当前 Arduino、Bluepad32、Bluepad32 Arduino adapter、BTstack 和 LVGL 仍然是为这个 PlatformIO/ESP-IDF 混合构建准备的兼容快照；它们的 CMake/Kconfig 差异尚未全部提取为独立 patch，因此暂不宣称“干净克隆后只靠锁文件即可复原”。

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
