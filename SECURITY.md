# Security Policy

## Supported versions

| Version | Supported |
|---------|-----------|
| 0.1.x   | Yes       |
| < 0.1   | No        |

## Reporting a vulnerability

If you discover a security issue (for example a crash or memory-safety bug when parsing untrusted images), please **do not** open a public GitHub issue.

Prefer one of:

1. GitHub Security Advisories for [localtools/ai-meta](https://github.com/localtools/ai-meta/security/advisories/new) (private report), or
2. Contact the maintainers via the organization listed on https://github.com/localtools

Include:

- Affected version / commit
- A minimal proof-of-concept file or steps to reproduce
- Impact (crash, heap corruption, unexpected overwrite, etc.)

We will acknowledge reports as soon as practical and coordinate a fix and disclosure timeline.

## Scope notes

- ai_meta parses untrusted binary formats; hardening against malicious inputs is a primary security goal.
- The library does **not** cryptographically verify C2PA signatures. Do not treat presence of a C2PA box as proof of authenticity.
