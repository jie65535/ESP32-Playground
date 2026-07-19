# Fusion Pixel Font 点阵字库管线

## 资产

Playground 保留以下可复用资产：

- `src/ui/BitmapFont.h/.cpp`
- `src/ui/BitmapFontData.h` 生成后的精简字库
- `tools/generate_bitmap_font.py`
- `tools/platformio_font_sync.py` 构建前自动同步脚本
- `tools/font_chars.txt`
- `licenses/FusionPixelFonts-OFL.txt`
- `tools/vendor-fonts/` 中的完整 BDF（本地保存、Git 忽略）

字体来源是 Fusion Pixel Font 和 Fusion Bold Pixel Font，采用 SIL Open Font License 1.1。完整许可证必须和生成结果一起保留。

## 为什么不用完整字体

ESP32 固件不需要几十 MB 的完整中文字库。先从界面文案收集字符，再从 BDF 生成精简数组，可以降低 Flash 占用并让缺字问题在构建时暴露。

## 关键排版经验

- 普通和粗体字都使用 16px 行盒。
- 保留 BDF 的 ascent、descent 和 advance，共享 baseline。
- 不要对每个汉字单独垂直居中，否则中英文混排会跳动。
- 动态数字应复用同一套 `BitmapFont` 度量；如果实验需要另一套字体，必须明确记录其行盒和 baseline 差异。
- 缺字不能静默显示空白；测试应检查所用 UTF-8 字符都在子集中。

## 英文、数字和符号

生成器会自动加入 ASCII `0x20–0x7E`，所以英文、数字和常见标点也可以使用 Fusion 字形。`BitmapFont` 保留为独立的 RGB565 画布渲染资产，并通过 `LvglBitmapFont` 适配层提供 `lv_font_t` 视图；两条路径共享同一份字形数据，不重新引入旧显示驱动。现有 Shell 仍按页面逐步迁移字体，Display/Color Lab 页面先作为中文像素字体验证入口。

如果以后确实需要更大的数字，只应在实验记录中明确说明为何引入另一套度量，不能让字体切换破坏现有行盒和 baseline。

## 重新生成

```powershell
python tools/generate_bitmap_font.py `
  --small-bdf tools\vendor-fonts\fusion-pixel-12px-proportional-zh_hans.bdf `
  --normal-bdf tools\vendor-fonts\fusion-bold-pixel-12px-proportional-zh_hans.bdf
```

生成器默认会把 `src/` 下 C/C++ 普通字符串字面量中的 UTF-8 字符加入子集，
因此新增页面文案后不需要手工把每个汉字再抄到 `font_chars.txt`。`font_chars.txt`
仍保留给共享符号、预置文案和不在当前源码中的实验字符。

PlatformIO 的 `pre` 脚本会在编译前比较源码字符集合和已生成字库：

- 本地 BDF 存在且发现缺字：自动重新生成 `BitmapFontData.h`。
- 本地 BDF 不存在且发现缺字：直接报出缺失字符并终止构建。
- 字符集合已经一致：不改写生成文件，避免无意义地触发全量编译。

生成后的 `BitmapFontData.h` 才进入固件，原始 BDF 不进入普通 Git 历史。
