# ai_meta

[![GitHub](https://img.shields.io/badge/github-localtools%2Fai--meta-blue)](https://github.com/localtools/ai-meta)

C11 library for **detecting, extracting, stripping, and writing** AI-generation metadata declared in image files.

**Repository:** https://github.com/localtools/ai-meta

This library only reads/writes **declared metadata**. It does **not** analyze pixels to guess whether an image is AI-generated.

## Supported (MVP)

| Scheme | Status | Notes |
|--------|--------|--------|
| PNG `tEXt` / `iTXt` / `zTXt` | Supported | Stable Diffusion `parameters`, prompts, etc. `zTXt`/compressed `iTXt` inflated via zlib; SD blobs expanded into discrete fields |
| EXIF (basic) | Supported | Software, UserComment, ImageDescription, Artist, Make/Model; JPEG APP1 + PNG `eXIf` + WebP `EXIF` |
| XMP (basic) | Supported | Presence + AI-marker heuristics (Firefly, generative DigitalSourceType, CreatorTool, …) |
| C2PA | Detect-only stub | JPEG APP11/JUMBF `c2pa` marker; full manifest/signature verification is a stretch goal |
| IPTC | Reserved | API bitmask only |

### Formats

- **PNG** — text chunks + optional `eXIf`
- **JPEG** — EXIF/XMP APP1; COM inject for write; C2PA detect
- **WebP** — RIFF `EXIF` / `XMP ` chunks

## API

Primary API is **buffer-based** (caller owns input). Optional `*_file` helpers perform I/O.

```c
ai_meta_scan_result scan;
ai_meta_scan(buf, len, &scan);

ai_meta_info *info = NULL;
ai_meta_extract(buf, len, &info);
/* use info->fields[]; then: */
ai_meta_info_free(info);

uint8_t *out = NULL; size_t out_len = 0;
ai_meta_strip(buf, len, AI_META_FLAG_STRIP_ALL_AI | AI_META_FLAG_KEEP_COLOR_PROFILE,
              &out, &out_len);
ai_meta_buffer_free(out);

ai_meta_write(buf, len, "parameters", "Steps: 20, Seed: 1", &out, &out_len);
ai_meta_buffer_free(out);
```

### Conventions

- **Errors**: `ai_meta_err` enum (0 = OK). Use `ai_meta_strerror()`.
- **Memory**: returned `ai_meta_info*` → `ai_meta_info_free()`; strip/write buffers → `ai_meta_buffer_free()`.
- **Strip/write**: always produce a **new buffer** (never in-place rewrite of the input pointer).
- **Color profiles**: kept when `AI_META_FLAG_KEEP_COLOR_PROFILE` is set (recommended default).

## Build

```bash
make          # static lib, CLI, fixtures, tests
make cli      # build/ai_meta
make test
```

**C11** + **zlib** (for PNG `zTXt` / compressed `iTXt`). Cross-platform (Linux/macOS/Windows with a C11 toolchain). Ships as **static** (`build/libai_meta.a`) and optional shared targets.

```bash
./build/ai_meta scan tests/fixtures/sd_parameters.png
./build/ai_meta extract tests/fixtures/sd_parameters.png
./build/ai_meta strip tests/fixtures/sd_parameters.png /tmp/clean.png
./build/ai_meta write tests/fixtures/clean.png /tmp/out.png parameters "Steps: 10"
```

## Design choices

- **Own I/O?** Optional. Core ops take buffers; file helpers wrap fread/fwrite.
- **Dependencies?** MVP is dependency-free. Optional future: zlib (zTXt), libexif, a C2PA SDK.
- **License:** MIT

## Out of scope

- Pixel / ML-based AI-image detection
- Full C2PA claim verification and signing
- Lossy re-encode of image payloads (strip/write only touch metadata containers)
- Guaranteed round-trip of every exotic EXIF MakerNote

## Test plan

`make test` builds fixtures under `tests/fixtures/`:

| Fixture | Expectation |
|---------|-------------|
| `clean.png` / `.jpg` / `.webp` | no AI schemes |
| `sd_parameters.png` | PNG_TEXT + likely_ai; strip removes it |
| `benign_text.png` | Title kept with `--keep-non-ai` / `KEEP_NON_AI_TEXT` |
| `ai_xmp.webp` | XMP + likely_ai |
| `truncated.png` | no crash; error or partial OK |

## Example

```bash
make
cc -Iinclude -o examples/basic examples/basic.c build/libai_meta.a -lz
./examples/basic tests/fixtures/sd_parameters.png
```

## Stretch goals

1. Full EXIF IFD rewrite (not only COM/XMP inject)
2. C2PA JUMBF parse + optional signature verify via upstream SDK
3. IPTC-IIM / Photoshop IRB blocks
