# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- `make dist` / `make package` — portable tarball (`include/`, `lib/`, `bin/`, pkg-config, license)
- GitHub Actions `release` workflow — build/test on Linux + macOS and attach assets to tag releases (`v*`)
- OS-aware `make shared` (`.so` on Linux, `.dylib` on macOS with `@rpath`)

### Documentation

- Expanded README for FOSS consumers (install, CLI, API, layout)
- Added CONTRIBUTING, SECURITY, CODE_OF_CONDUCT, and this changelog
- Documented release packaging and GitHub Release assets

## [0.1.0] — 2026-08-09

### Added

- Public C11 API: `ai_meta_scan`, `ai_meta_extract`, `ai_meta_strip`, `ai_meta_write` (+ file helpers)
- PNG `tEXt` / `iTXt` / `zTXt` (zlib inflate), SD parameter expansion
- Basic EXIF and XMP extraction; JPEG COM read/write; WebP EXIF/XMP
- JPEG IPTC via Photoshop APP13 IRB
- C2PA detect/strip/hints for PNG `caBX` and JPEG APP11
- CLI tool `ai_meta`, pkg-config template, Makefile install, CMakeLists, CI
- Example `examples/basic.c`

### Notes

- C2PA signature verification is intentionally out of scope for 0.1.x
- Pixel-based AI detection is out of scope

[Unreleased]: https://github.com/localtools/ai-meta/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/localtools/ai-meta/releases/tag/v0.1.0
