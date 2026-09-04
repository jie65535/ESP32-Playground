# 实验 017：PGOS Shell 架构收敛、图标与汉化

## 目标

在外设页和三款小游戏已经让 Launcher 超过一屏后，回收 Shell 的导航层级、
重复实现和视觉资源，并验证现有 Fusion Pixel 字库可承担系统中文正文。

## 架构改造

- 首页只保留“游戏 / 设置 / 系统工具”三类入口；三个父菜单和桌面复用同一个
  数据驱动 `MenuApp`。
- `AppManager` 保存最多四层父页面历史。菜单 Confirm Push，Back Pop，Home
  清空历史；USB/TCP `page ...` 继续无历史直达。
- Snake、Tetris、Breakout 的三套同构分数服务合并为 `GameScoreService`，仍
  使用 `pgos_snake` schema 2、`pgos_tetris` schema 1、`pgos_breakout`
  schema 1。旧 Snake namespace 没有 schema key 时先读取原分数再补 key。
- 卡片图标通过 `UiIcon` 访问固定版本 Lucide A8 资源；状态栏继续使用 LVGL
  内置的 Wi-Fi/蓝牙等通用符号。

## 汉化和排版边界

- 桌面、父菜单、设置页、系统监视、时钟、颜色实验和网络状态改为中文正文。
- `PGOS`、`Wi-Fi`、`RGB`、`RTC`、芯片名、协议命令等标志或技术名称保留。
- 24px 页面标题和依赖大字号固定度量的三款游戏 HUD 保留英文；中文正文使用
  Fusion Pixel/Bold Pixel 的 16px 行盒。
- 真机首轮截图发现 40px 卡片的副标题行盒正好落到底边。最终统一为 44px
  卡片和 52px 步距（8px 卡间距），图标垂直居中；无页级副标题时首卡
  y=50，需要页级说明时首卡 y=64。
- 副标题改为可选：菜单和普通设置项只保留标题与当前值；连接状态、硬件
  诊断、扫描时限、配对保留规则和必须展示的操作命令继续使用第二行。
- 字形边界检查确认中文有效像素位于 16px 行盒第 3–13 行；视觉偏低来自焦点
  边框改变内容区原点。最终固定 2px 边框，单行标题按卡片中线对齐，两行
  内容使用 y=2/21 围绕中线排布。

## 图标资产

- 来源：[`lucide-static 0.468.0`](https://github.com/lucide-icons/lucide)，ISC License。
- 生成：`python tools/generate_ui_icons.py`。
- 固件格式：15 个 24×24 A8 遮罩，由 LVGL 按焦点状态动态着色。
- 不引入 SVG 解析器、完整图标字体或运行时堆分配。

## 验证

2026-07-20，ES3N28P / COM3：

- `python -m unittest discover -s tools/tests -p "test_*.py"`：20 项通过。
- 最终烧录构建：RAM 78,696 / 327,680 bytes（24.0%），Flash
  1,761,657 / 6,553,600 bytes（26.9%）。
- esptool 烧录并校验成功，硬复位后 USB 截图协议正常。
- 采集首页、Games、Settings、System Tools、System、Clock、Color Lab、
  Display、Sound、RGB Light、Controller、Remote Control、Network、Snake、
  Tetris、Breakout 共 16 张 320×240 截图。
- 逐张检查未见中文空白、乱码、自定义图标空白或卡片文字越界；普通菜单和
  设置项使用单行标题，状态/限制/特殊逻辑保留副标题；游戏 HUD 按既定边界
  保留英文。
- 从 System 按 Back 返回 System Tools，确认父页面历史和 Backward 转场生效。

本地截图保存在被 Git 忽略的 `captures/017_shell_localization/`，不把批量 PNG
加入普通仓库历史。

2026-07-25，COM3 复测发现 Games 菜单新增 2048 后已有 7 项，但 `MenuApp`
仍只分配 6 张 `UiCard`。创建第 7 项会越界 36 字节，覆盖紧邻的 Settings
菜单对象 vtable；随后按 B 调用虚函数时出现 `InstructionFetchError`，并非栈溢出。
现将容量提升到 8，所有菜单循环使用夹紧后的项目数，并加入四个菜单的编译期容量
断言。干净构建、50 项主机测试和 COM3 烧录通过；USB 连续执行 Games → 游戏 →
Back → Games → Back → Desktop，以及 Platformer → Back → Desktop，设备均未复位。

## 后续

1. 在真机实体屏上继续确认像素字体的远距离可读性和卡片密度。
2. 需要中文大标题或游戏 HUD 时，单独评估更大字号的 Fusion 子集与 Flash
   预算，不对 12px 字形做简单整数放大。
3. `AppContext` 的服务面仍较宽；等主机 FakeService 测试建立后，再按能力拆成
   更窄的 App 依赖，而不是引入全局 Service Locator。
