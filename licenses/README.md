# 第三方字体资源

`src/ui/BitmapFontData.h` 是从以下两套 12px 简体中文比例 BDF 字体生成的精简点阵字库，仅包含 Playground 实验界面实际需要的字符：

- [Fusion Pixel Font](https://github.com/TakWolf/fusion-pixel-font)：普通文字。
- [Fusion Bold Pixel Font](https://github.com/pixel-font-studio/fusion-bold-pixel-font)：强调文字。

两者均采用 SIL Open Font License 1.1，版权声明和许可证全文见 `FusionPixelFonts-OFL.txt`。仓库不提交数十 MB 的原始完整 BDF，只提交生成后的字符子集；本机可将完整 BDF 放在被忽略的 `tools/vendor-fonts/` 中重新生成。

当前使用两套字体的 `2026.07.01` 字体包。完整 BDF 可以放在已被 Git 忽略的 `tools/vendor-fonts/`，然后执行：

```powershell
python tools/generate_bitmap_font.py `
  --small-bdf tools\vendor-fonts\fusion-pixel-12px-proportional-zh_hans.bdf `
  --normal-bdf tools\vendor-fonts\fusion-bold-pixel-12px-proportional-zh_hans.bdf
```

生成器直接保留 BDF 的 advance、ascent 和 descent，不再按每个字符的独立包围盒垂直居中，因此中英文混排共享 baseline，复杂汉字也不会被 12px 方框裁切。
