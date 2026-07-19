# 统一字体、比例 advance 与动态数值

## 当前决定

Fusion Pixel Font 生成的 BitmapFont 同时支持 RGB565 画布和 LVGL
`lv_font_t` 两种绘制路径：

- 普通信息：Fusion Pixel Font 12px proportional。
- 标题和动态值：Fusion Bold Pixel Font 12px proportional。
- 所有字形共享 16px 行盒和 baseline。
- ASCII、中文和符号由同一个 UTF-8 解码器处理。

当前页面不使用 TFT_eSPI Font2。Color Lab 已作为第一处 LVGL 中文验证入口；
其它 PGOS 页面仍主要使用 Montserrat，后续迁移时必须显式选择字体，不能假定
整个 Shell 已经统一为 Fusion 字体。

## 为什么使用比例字体

英文字符的 advance 可以小于汉字，文本更紧凑；汉字仍保持稳定的 12px 字宽。布局不能假设每个字符都是相同宽度，应使用 `textWidth()` 计算居中和右对齐位置。

## 动态数据规则

- 数字变化时保持同一字体和 baseline。
- 单位、负号、小数点和百分号必须在字符子集中。
- 数据区域先按最大可见宽度设计，再用实际 advance 对齐。
- 缺字测试属于 Python 回归的一部分，不等到真机才发现空白。
