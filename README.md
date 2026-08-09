# ai_meta

[![CI](https://github.com/localtools/ai-meta/actions/workflows/ci.yml/badge.svg)](https://github.com/localtools/ai-meta/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Language: C11](https://img.shields.io/badge/language-C11-informational.svg)](#build)

**ai_meta** is a small C11 library (and CLI) for detecting, extracting, stripping, and writing **AI-generation metadata** declared in image files.

- **Repository:** https://github.com/localtools/ai-meta
- **License:** [MIT](LICENSE) — free and open source
- **Scope:** declared metadata only — it does **not** analyze pixels to guess whether an undeclared image is AI-generated

## Why

Generators and platforms increasingly embed provenance and generation parameters (Stable Diffusion `parameters`, EXIF/XMP, C2PA Content Credentials, IPTC). This library gives applications a dependency-light way to inspect and scrub that metadata while leaving pixel data and color profiles intact when requested.

## Features

| Scheme | Status | Notes |
|--------|--------|--------|
| PNG `tEXt` / `iTXt` / `zTXt` | Supported | SD/A1111-style `parameters`; zlib inflate for compressed text; field expansion (`prompt`, `Steps`, `Seed`, …) |
| EXIF (basic) | Supported | Software, UserComment, ImageDescription, Artist, Make/Model — JPEG APP1, PNG `eXIf`, WebP `EXIF` |
| XMP (basic) | Supported | Property hints (`CreatorTool`, `DigitalSourceType`, …) + AI heuristics |
| C2PA | Detect / strip / hints | PNG `caBX` JUMBF + JPEG APP11; generator / `trainedAlgorithmicMedia` hints; **no** signature verification yet |
| IPTC | Supported (JPEG) | Photoshop APP13 IRB `0x0404` — Keywords, Caption, Headline, Byline |

**Formats:** PNG, JPEG, WebP.

## Quick start

### Requirements

- C11 compiler (`cc`, `clang`, or `gcc`)
- [zlib](https://zlib.net/) development headers (`zlib1g-dev` on Debian/Ubuntu; usually present on macOS)
- `make` (optional: CMake 3.16+)

### Build & test

```bash
git clone https://github.com/localtools/ai-meta.git
cd ai-meta
make          # static lib, CLI, fixtures, tests
make test
```

Artifacts land in `build/`:

| Path | Description |
|------|-------------|
| `build/libai_meta.a` | Static library |
| `build/libai_meta.dylib` / `.so` | Shared library (`make shared`) |
| `build/ai_meta` | Command-line tool |

### Release package (local)

```bash
make dist    # or: make package
```

Creates `dist/ai_meta-<version>-<os>-<arch>.tar.gz` with headers, static/shared libraries, CLI, relocatable `pkg-config` file, and license docs.

GitHub Releases are built automatically when you push a version tag (`v*`). Assets include Linux x86_64 and macOS arm64 tarballs plus `SHA256SUMS`.

```bash
git tag v0.1.0
git push origin v0.1.0
# → Actions workflow "release" builds, tests, and attaches artifacts
```

### Install (system-wide)

```bash
sudo make install          # PREFIX=/usr/local by default
pkg-config --cflags --libs ai_meta
```

```bash
sudo make uninstall
```

### CLI

```bash
./build/ai_meta scan path/to/image.png
./build/ai_meta extract path/to/image.png
./build/ai_meta strip path/to/in.png path/to/out.png [--keep-non-ai]
./build/ai_meta write path/to/in.png path/to/out.png parameters "Steps: 10, Seed: 1"
```

### Link against the library

```bash
cc -Iinclude -o app app.c build/libai_meta.a -lz
# or after install:
cc $(pkg-config --cflags ai_meta) -o app app.c $(pkg-config --libs ai_meta)
```

See [`examples/basic.c`](examples/basic.c).

## C API

Public header: [`include/ai_meta.h`](include/ai_meta.h).

Primary API is **buffer-based** (caller owns input). Optional `*_file` helpers perform I/O.

```c
#include "ai_meta.h"

ai_meta_scan_result scan;
if (ai_meta_scan(buf, len, &scan) == AI_META_OK) {
    /* scan.format, scan.schemes (bitmask), scan.likely_ai */
}

ai_meta_info *info = NULL;
if (ai_meta_extract(buf, len, &info) == AI_META_OK) {
    for (size_t i = 0; i < info->field_count; i++)
        printf("%s = %s\n", info->fields[i].key, info->fields[i].value);
    ai_meta_info_free(info);
}

uint8_t *out = NULL;
size_t out_len = 0;
unsigned flags = AI_META_FLAG_STRIP_ALL_AI | AI_META_FLAG_KEEP_COLOR_PROFILE;
if (ai_meta_strip(buf, len, flags, &out, &out_len) == AI_META_OK) {
    /* write out[0..out_len) */
    ai_meta_buffer_free(out);
}

if (ai_meta_write(buf, len, "parameters", "Steps: 20, Seed: 1", &out, &out_len) == AI_META_OK)
    ai_meta_buffer_free(out);
```

### Conventions

| Topic | Rule |
|-------|------|
| Errors | `ai_meta_err` (0 = OK); `ai_meta_strerror()` |
| Memory | `ai_meta_info_free()` / `ai_meta_buffer_free()` for library-allocated results |
| Strip / write | Always return a **new** buffer — input is never mutated in place |
| Color profiles | Preserved when `AI_META_FLAG_KEEP_COLOR_PROFILE` is set (recommended) |

## Design

- **I/O:** optional — core ops take buffers; `*_file` helpers wrap stdio
- **Dependencies:** zlib only (compressed PNG text); no libpng / libjpeg / libexif required
- **Portability:** Linux, macOS; Windows via a C11 toolchain (MSVC/MinGW) with zlib
- **Safety:** malformed / truncated metadata should fail gracefully (no crash); see truncation smoke tests

## Out of scope

- Pixel / ML-based “is this AI?” detection
- Full C2PA claim verification and signing
- Lossy re-encode of image payloads (metadata containers only)
- Guaranteed round-trip of every exotic EXIF MakerNote

## Testing

```bash
make test
```

Fixtures are generated under `tests/fixtures/` (gitignored). Coverage includes clean images, SD `parameters`, benign text keep, XMP, IPTC, zTXt inflate, truncation smoke, and round-trip write.

CI runs on Ubuntu and macOS: [`.github/workflows/ci.yml`](.github/workflows/ci.yml).

## Project layout

```
include/ai_meta.h     Public API
src/                  Library implementation
tools/ai_meta_cli.c   CLI
examples/basic.c      Minimal consumer
tests/                Unit tests + fixture generator
ai_meta.pc.in         pkg-config template
```

## Contributing

Contributions are welcome. See [CONTRIBUTING.md](CONTRIBUTING.md) and our [Code of Conduct](CODE_OF_CONDUCT.md).

For security issues, see [SECURITY.md](SECURITY.md) — please do not open public issues for vulnerabilities.

## Changelog

See [CHANGELOG.md](CHANGELOG.md).

## License

This project is licensed under the **MIT License** — see [LICENSE](LICENSE).

```
Copyright (c) 2026 Local Tools and ai_meta contributors
```

You may use, modify, and distribute this software under the terms of that license.
