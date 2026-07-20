# 统一字体、比例 advance 与动态数值

## 当前决定

Fusion Pixel Font 生成的 BitmapFont 同时支持 RGB565 画布和 LVGL
`lv_font_t` 两种绘制路径：

- 普通信息：Fusion Pixel Font 12px proportional。
- 标题和动态值：Fusion Bold Pixel Font 12px proportional。
- 所有字形共享 16px 行盒和 baseline。
- ASCII、中文和符号由同一个 UTF-8 解码器处理。

当前页面不使用 TFT_eSPI Font2。2026-07-20 起，PGOS Shell 卡片、正文和设置值
已显式使用 Fusion 字体；24/28px 页面标题和固定大字号游戏 HUD 仍使用
Montserrat。新增中文控件必须使用 `UiRuntime::createBodyLabel()` 或
`applyBodyFont()`，不能假定任意 `lv_label` 会自动选择中文字体。

## 为什么使用比例字体

英文字符的 advance 可以小于汉字，文本更紧凑；汉字仍保持稳定的 12px 字宽。布局不能假设每个字符都是相同宽度，应使用 `textWidth()` 计算居中和右对齐位置。

## 动态数据规则

- 数字变化时保持同一字体和 baseline。
- 单位、负号、小数点和百分号必须在字符子集中。
- 数据区域先按最大可见宽度设计，再用实际 advance 对齐。
- 缺字测试属于 Python 回归的一部分，不等到真机才发现空白。

## 卡片行盒

两行卡片不能继续使用 40px 高度：标题和副标题各占 16px 行盒，副标题从
y=24 开始时会正好贴住底边。当前 Shell 统一使用 44px 卡片和 52px 步距，
在卡片内部留下 4px 底边距，并在相邻卡片间保留 8px。无页级副标题时首卡
y=50；确实需要页级说明时首卡 y=64。

副标题不是默认槽位。标题已经表达完整含义时不创建副标题，标题在卡片内
垂直居中；只在连接状态、硬件诊断、操作限制或数据保留等特殊逻辑无法由
标题和当前值表达时使用第二行。

Fusion 12px 中文字形的有效像素通常位于 16px 行盒第 3–13 行，字形数据本身
已接近垂直居中。卡片边框必须固定为 2px，不能在焦点态从 1px 改成 2px，
否则 LVGL 内容区原点会移动并让文字跳动。单行标题使用 `LV_ALIGN_LEFT_MID`
对齐；两行卡片的标题/副标题 y=2/21，使两行可见像素围绕卡片中线分布。
