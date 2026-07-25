# SpeexDSP subset

This directory vendors the fixed-point preprocessing and MDF echo-canceller
dependencies from `xiph/speexdsp` commit
`7a158783df74efe7c2d1c6ee8363c1e695c71226` (SpeexDSP 1.2.1 lineage).

Only the BSD-licensed sources needed by the microphone spectral denoiser and
its optional echo-state interface are included. The ESP32 build uses the
fixed-point KISS FFT backend; floating-point control APIs are disabled.

The upstream `LICENSE` and `AUTHORS` files are preserved beside the sources.
On ESP32, `os_support_custom.h` allocates the long-lived processing workspace
from PSRAM during audio-service startup so Wi-Fi retains internal heap and no
allocation occurs when entering the microphone page. Other than this allocator
hook and whitespace-only normalization, the vendored algorithm sources are
unchanged.
