# Contributing to ai_meta

Thanks for helping improve this project. ai_meta is MIT-licensed free software; contributions are welcome under the same license.

## Development setup

```bash
git clone https://github.com/localtools/ai-meta.git
cd ai-meta
make test
```

You need a C11 compiler, `make`, and zlib development headers.

## Workflow

1. Open an issue for larger changes when practical (design discussion helps).
2. Fork the repository and create a topic branch from `main`.
3. Keep changes focused — prefer small, reviewable PRs.
4. Run `make test` (and `make shared` if you touch the build system).
5. Match existing style: C11, clear error returns, no crashes on truncated input.
6. Update `README.md` / `CHANGELOG.md` when you change user-visible behavior or the public API.
7. Open a pull request against `main` with a short description of *why*.

## Code guidelines

- Public API lives in `include/ai_meta.h` — keep ABI-ish stability when possible (document breaks in CHANGELOG).
- Prefer buffer-based APIs; file helpers are convenience wrappers.
- Do not add heavy dependencies without discussion. zlib is the only required third-party library today.
- Malformed metadata must not abort; return `ai_meta_err` and continue when safe.
- No secrets or large binary fixtures in git (`temp/` and `tests/fixtures/` are ignored).

## Commit messages

Use concise, imperative subjects, for example:

- `feat: detect C2PA in PNG caBX chunks`
- `fix: handle truncated JPEG APP1 lengths`
- `docs: clarify strip memory ownership`

## License of contributions

By submitting a contribution, you agree it is licensed under the MIT License (same as this repository), and that you have the right to submit it under those terms.

## Conduct

Participation is governed by [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).
