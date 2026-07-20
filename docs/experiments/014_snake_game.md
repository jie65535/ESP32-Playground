# 实验 014：PGOS 贪吃蛇小游戏

## 目标

在 PGOS 中加入第一款独立小游戏，验证前台 App 生命周期、统一手柄输入、固定步进逻辑和游戏专用绘制区域。

## 当前实现

- App：`SnakeApp`
- 画布：状态栏下方的 320×218 自绘区域，使用 LVGL draw layer，不直接访问 TFT/SPI。
- 棋盘：20×12，单格 15 像素。
- 输入：D-pad 或左摇杆改变方向，A 开始/暂停/重开，B/Home 返回桌面。
- 逻辑：固定步进 220 ms 起步，随每 4 个食物逐步加速到 90 ms；禁止立即反向。
- 摇杆：加入主轴差值门限，并限制每个蛇格最多接受一次转向，避免摇杆
  抖动在一次移动前覆盖多个方向。
- 反馈：吃到食物和结束时调用 AudioService；连接手柄时请求短震动。
- 入口：Launcher 的 Snake 卡片、USB/TCP `page snake` 命令。
- 持久化：`SnakeScoreService` 使用独立 `pgos_snake` NVS namespace 保存
  Top 5，带 schema=2；游戏运行中的新纪录只在结束或离开应用时写入。

> 2026-07-20：实验 017 将同构实现合并为 `GameScoreService`，上述
> `pgos_snake` namespace、schema=2 和写入时机保持不变。

## 构建验证

2026-07-19：

- `pio run -e playground` 通过。
- RAM：约 76.7 KB / 327.7 KB。
- Flash：约 1.714 MB / 6.554 MB 应用空间。
- Python 主机测试：14 项通过。

## 待真机验证

1. Launcher 进入 Snake，A 启动，B/Home 返回。
2. Xbox 手柄 D-pad 与左摇杆方向切换、快速连续转向和禁止反向。
3. 撞墙、撞自身、吃食物、加速、暂停和 Game Over。
4. LCD DMA 刷新稳定性、镜像画面和音效/震动反馈。

## 后续候选

- 排名并列规则、清空榜单入口和更丰富的结算动画。
- 关卡边界/穿墙模式和暂停菜单。
- 更细的脏矩形刷新统计。
- 在此基础上复用 RenderSurface 机制实现打砖块。
