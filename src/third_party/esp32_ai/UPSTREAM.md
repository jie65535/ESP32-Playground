# esp32-ai source and license

- Upstream repository: <https://github.com/slvDev/esp32-ai>
- Source snapshot used by PGOS:
  `9c4a214bdef2f9779bdc927b582d4061dd1ae362`
- MIT license added upstream by:
  `74744182329f08d7a1badc97e47576ef527532a0`
- Upstream copyright: Copyright (c) 2026 Viacheslav Sierbov

Commit `7474418` adds only the upstream `LICENSE`; it does not change the
source tree from `9c4a214`. PGOS therefore keeps `9c4a214` as the code import
baseline and preserves the MIT text from `7474418` verbatim in `LICENSE`.

## PGOS-local changes

The imported source is not an unmodified upstream checkout. At the time this
notice was added, the following tracked files differed from the source
snapshot or were added locally:

- `.gitignore`
- `CLAUDE.md`
- `RESULTS.md`
- `data/prepare_zh.py`
- `firmware/esp32_llm/vocab.h`
- `firmware/host_verify/verify.c`
- `src/gen_assets_zh.py`
- `src/train.py`

These changes provide the Chinese training/export workflow, generated
vocabulary, host verification, experiment notes, and local workflow support.
PGOS runtime integration outside this directory is maintained separately.

## License boundary

The bundled MIT license covers the software obtained from `esp32-ai`. It does
not by itself grant rights to datasets, model weights, generated training
artifacts, or unrelated third-party dependencies. PGOS does not include an
upstream pretrained model; its current model is trained and exported locally.
The TinyStories-Zh-1M dataset and any other training inputs must be reviewed
under their own terms.
