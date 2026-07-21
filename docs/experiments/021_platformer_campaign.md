# 实验 021：PGOS 超级马里奥 32 关战役

> 日期：2026-07-21
> 状态：32 关地图、战役推进、主要关卡机制、固定实体池和继续进度已实现；主机测试、固件构建、COM3 烧录及代表性分区截图通过

## 目标

在 320×240、16MB Flash、8MB PSRAM 的 PGOS 约束内，学习本地 C++ 参考工程
`G:\Temp\Super-Mario-Bros` 的地图组织和游戏状态，实现从 1-1 到 8-4 的完整可推进战役。
参考工程只作为主机端输入和行为依据；设备不加载其 SDL2 运行时、PNG、XML 或 CSV。

## 离线数据管线

- `tools/convert_smb_campaign.py` 读取 32 份属性文件和 192 个 CSV 图层，校验所有关卡均可往返解码，再生成 `PlatformerCampaignData.generated.cpp`。
- 六层地图按行独立 RLE；每个记录为 1-byte 长度和 2-byte tile id，空格使用 `0xffff`。全部地图、行偏移和属性数据的有效载荷为 86,883 bytes。
- 转换器修复参考工程 World 3-1 中一条截断的藤蔓属性记录：缺失字段只从该关默认入口补齐，修复规则留在转换器中，生成结果仍可重复。
- `tools/convert_smb_tile_assets.py` 将 block、enemy、player 三张图集转换为局部调色板和位打包索引；像素索引总计 300,160 bytes，设备端按需解码成 RGB565。
- 参考目录中的源文件不复制进项目；固件只编译生成的 C++ 常量。重新生成是显式主机步骤，不发生在 PlatformIO 构建中。

## 运行时边界

- `PlatformerLevelRuntime` 按视口查询 RLE 地图，并以最多 64 项稀疏修改记录 Used/Broken 图块，不解压整关地图。
- `PlatformerTileRenderer` 顺序扫描视口内的行 RLE，并将三张位打包图集一次解码到 PSRAM 缓存，再把当前 320×218 游戏区域合成到一块仅在应用活动时申请的 PSRAM RGB565 framebuffer；旧 3392px 预合成 1-1 背景不再链接。
- 引擎继续使用 8ms 固定步进和有界追赶；实体全部使用固定池：20 个敌人、20 个移动平台、12 个火焰棒、12 个敌方投射物、6 个道具和 2 发玩家火球。
- 最大 400×51 出生网格只保存固定 bitset，避免重复生成已离开再返回视口的敌人；运行中不按地图规模分配容器。
- `PlatformerProgressService` 使用 `pgos_mario` NVS namespace，只在过关扩展进度或刷新最高分时写入 continue world/stage、best score 和 complete 标记。

## 已实现战役机制

- 1-1 至 8-4 共 32 关、地上/地下/城堡/水下区域、跨区域和跨关水管、自动地下开场。
- 问号箱、普通砖、多金币砖、蘑菇、火花、星星、隐藏 1UP、金币、可破坏砖和火球。
- 藤蔓生长、攀爬和天空奖励区；移动平台、上下/水平平台、滑轮联动、单向落脚、弹簧床和旋转火焰棒。
- Goomba、Koopa/Paratroopa、Buzzy、Piranha、Blooper、Cheep、Lakitu/Spiny、Hammer Bro、Bullet Bill、Lava Bubble 和 Bowser 的差异化状态机。
- 敌人踩踏、龟壳滑行、火球免疫/伤害、无敌星伤害保护、Bowser 五次火球生命、斧头触发的桥面逐段坍塌。
- 旗杆、城堡自动行走、时间奖励、死亡/重生、Game Over、关卡推进、8-4 循环传送点和最终通关状态。
- 标题页默认选中 NVS 继续关卡并显示最高分，同时按参考项目 `MenuSystem` 开放全部 32 关：左右选择 WORLD/STAGE 字段，上下在 1–8/1–4 内增减，A 启动所选关卡；选择过程不写 NVS。USB `platformer maptest [world-stage]` 仍用于无状态地图巡检，`platformer mapnext` 提供无需手柄的逐段移动。

## 性能与参考行为复核

- 最初的完整战役渲染仍经 LVGL 自绘回调逐图块解码，真机 UI 阶段约 128.7ms/frame，仅约 7.8 FPS，并且固定步进追赶上限令移动和加速一起变慢。运行态现改为 PSRAM framebuffer 直接合成及 LCD DMA 提交，Shell 和非运行页面仍归 LVGL 所有。
- 稳定方案在 40MHz SPI2 上提交 6 个 320×40 区块。2026-07-21 最终状态为 SPI 33.8ms、整帧 38.5ms、DMA 等待 31.2ms，用户实测交互明显流畅。曾尝试从 PSRAM 单次提交 320×218，真机报 `setup_dma_priv_buffer: Failed to allocate priv TX buffer`；已回退并固定 40 行分块，不应在未解决内部 DMA bounce buffer 前重试整帧提交。
- 对照参考工程 `PhysicsSystem.cpp` 的 `TILE_ROUNDNESS=4`（其画面为 2 倍缩放），玩家落下时双方各收窄 1px、上顶时各收窄 2px，水平碰撞底部裁去 4px。当前引擎只对玩家与地图采用等比例方向碰撞，敌人和道具仍使用完整 AABB；1-2 的单格竖井和恰好两格高通道已有真实地图回归测试。
- 参考工程下蹲时不移动两格高的绘制实体，而把碰撞箱改为下半格：缩放回本项目即 `y=16, h=16`。当前引擎采用等价的一格高蹲姿碰撞，并分别锚定 32px 战役图块和 22px 裁切素材的脚底。下蹲受伤会先按当时的实际碰撞高度保存脚底，再清除蹲姿并缩成 16px 小马，避免受伤锁定期间下沉一格。
- 参考工程的 `shrink()` 使用 45 tick 冻结动画，随后以 `EndingBlinkComponent(10, 150)` 每 10 帧切换显隐并持续 150 帧；碰撞代码直接以 `FrozenComponent`/`EndingBlinkComponent` 判定免伤，因此 0.75 秒变身提示和之后 2.5 秒闪烁与总计 3.25 秒免伤严格同步。当前引擎采用相同分段：变身结束的同一步开始每约 167ms 切换显隐，最后一次闪烁结束的同一步恢复可受伤；锁定结束后可在闪烁期间控制逃离。参考回调没有同时显式重置蹲姿 `hitbox.y` 或保存脚底，因此这里只学习分离绘制框/碰撞箱的结构，不复制该收尾缺口。
- 隐藏砖揭示前可从上方穿过、揭示后成为实体；火花不水平移动；食人花按显露/下降/隐藏/上升周期原地活动；敌人被踩扁后立即退出碰撞。一次同时踩多个敌人先统一判定踩踏，避免同一步的侧碰伤害。
- 龟壳启动和停止不重复加分，只有首次踩踏及龟壳击杀得分；龟壳停止是参考行为。Koopa/Buzzy 入壳时保持脚底，渲染不再套用站立 Koopa 的 8px 高度补偿，敌人朝向跟随运动方向。
- 玩家和水管/旗杆的前后景顺序、旗杆动态锚点、相邻砖块上顶选择、变大/下蹲脚底锚定、管道遮挡及普通/火焰姿态颜色均按参考状态路径复核并加入对应回归断言。
- 参考项目并非固定从某一关开始：其 `MenuSystem` 提供 1–8 世界和 1–4 子关的完整 Level Select，左右移动字段下划线、上下修改当前数值、确认后启动。PGOS 标题页采用相同字段语义，并额外以所选关卡首屏作为即时预览。

## 验证

```text
python -m unittest discover -s tools/tests -p "test_*.py" -v
pio run -e playground
pio run -e playground -t upload --upload-port COM3
```

40 项 Python 测试入口全部通过。Platformer 原生套件覆盖 32 关索引/RLE 往返、首屏渲染、碰撞查询、稀疏修改、关卡推进、水管和 8-4 循环点、水下移动、自动地下开场、藤蔓、弹簧、移动平台/滑轮、火焰棒、城堡桥、主要敌人状态机、单格缝隙方向碰撞以及下蹲受伤脚底锚定。

最终烧录构建资源为 RAM 87,544 / 327,680 bytes（26.7%），Flash 2,354,777 / 6,553,600 bytes（35.9%），image 2,355,033 bytes。相比扩展初版的 3,757,277-byte 固件仍减少约 1.40MB。新增空间主要用于完整姿态/敌人逻辑、选关界面与直接渲染路径；解码图块缓存位于 PSRAM。32 个首屏的主机端 contact sheet 位于 `captures/platformer/campaign-first-screens.png`，已检查天空、地下、水下、城堡和奖励区的图层组合，无空白帧或明显错位。

最终固件已烧录 COM3，esptool 连接到 ESP32-S3、写入全部四段镜像并通过 SHA 校验。设备端截图已确认：

- `campaign-device-final-title.png`：完整战役标题、continue 1-1 与 best score 正常。
- `campaign-device-final-1-1.png`：普通 1-1 开局、Mario、HUD 和倒计时正常。
- `campaign-device-maptest-1-2-underground-fixed.png`：1-2 的 Y13 地下分区使用黑底并正确叠加地下砖层。
- `campaign-device-maptest-8-4-castle.png`：8-4 城堡首屏的黑底、灰砖、熔岩和火焰棒正常。
- `campaign-device-maptest-8-4-water.png`：8-4 的 X256/Y13 水下分区自动切换蓝底，图层连续。

首次设备巡检发现无状态 maptest 只使用关卡级背景色，导致 1-2 地下错误显示蓝底。正常游戏的 warp 路径原本已通过 `PlatformerLevelRuntime::setSection` 切换背景；本轮进一步让 maptest/主机预览按入口、同关水管和藤蔓相机锚点选择分区背景，并为 1-2 地下和 8-4 水下增加整帧回归测试。为缩短 32 关诊断路径，控制台同时新增 `platformer maptest 8-4` 形式的关卡直达参数。

## 验收边界

- 自动测试确认数据完整、关键状态可达以及资源上限，但不等于逐像素、逐帧复刻原作。
- 参考工程自身的个别地图属性不完整，转换器只做了可追溯的最小修复；后续发现其它源数据差异时应继续记录，不应在运行时静默猜测。
- 实体 Xbox 的连续跑跳手感、全部 32 关长局流程、声音/震动节奏及数小时稳定性仍需人工长测。发现问题时优先补最小复现测试，再修改共享机制。
- 2026-07-21 最新固件已烧录 COM3 并通过 SHA 校验；板端截图 `artifacts/platformer_crouch_damage_fix.png` 显示 1-2 运行画面正常。下蹲受伤的脚底不变量已由主机测试覆盖，仍需实体手柄实际触发一次确认变身动画的主观观感。
- 选关页已在 COM3 验证默认 1-2、切回并启动 1-1、选择并启动 2-1，以及上界 8-4；对应截图为 `artifacts/platformer_level_select_default.png`、`platformer_level_select_1_1.png`、`platformer_level_select_2_1_running.png` 和 `platformer_level_select_8_4.png`。
