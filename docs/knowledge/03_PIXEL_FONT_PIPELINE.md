# Fusion Pixel Font 点阵字库管线

## 资产

Playground 保留以下可复用资产：

- `src/ui/BitmapFont.h/.cpp`
- `src/ui/BitmapFontData.h` 生成后的精简字库
- `tools/generate_bitmap_font.py`
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
- 动态数字仍可使用 TFT_eSPI Font2，避免为每个数字重复生成点阵。
- 缺字不能静默显示空白；测试应检查所用 UTF-8 字符都在子集中。

## 英文、数字和符号

生成器会自动加入 ASCII `0x20–0x7E`，所以英文、数字和常见标点也可以使用 Fusion 字形。当前 Playground 页面统一通过 `BitmapFont` 绘制：标签使用普通体，标题和动态数值使用粗体；不再让同一页面混用 TFT_eSPI Font2 和 Fusion 字体。

如果以后确实需要更大的数字，只应在实验记录中明确说明为何引入另一套度量，不能让字体切换破坏现有行盒和 baseline。

## 重新生成

```powershell
python tools/generate_bitmap_font.py `
  --small-bdf tools\vendor-fonts\fusion-pixel-12px-proportional-zh_hans.bdf `
  --normal-bdf tools\vendor-fonts\fusion-bold-pixel-12px-proportional-zh_hans.bdf
```

生成后的 `BitmapFontData.h` 才进入固件，原始 BDF 不进入普通 Git 历史。
