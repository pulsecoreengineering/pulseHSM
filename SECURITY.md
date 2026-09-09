# Security Policy

## Supported versions

| Version | Supported |
|---------|-----------|
| 1.2.x   | Yes       |
| < 1.2   | No        |

## Reporting a vulnerability

PulseHSM is an embedded library with no network stack, no authentication
layer, and no external dependencies. Its attack surface is limited to code
that runs locally on a microcontroller under the developer's direct control.

If you discover a vulnerability (e.g., a buffer overrun in the event queue,
an integer overflow in the state table, or a race condition in the ISR path),
please report it **privately** before disclosing it publicly:

1. Open a [GitHub Security Advisory](../../security/advisories/new) on this
   repository (preferred — keeps the report private until a fix is released).
2. Alternatively, e-mail the maintainers directly. Their address can be found
   in the commit history.

Please include:

- A description of the vulnerability.
- Steps to reproduce (minimal sketch or test case).
- The affected version(s).
- Any suggested fix, if you have one.

We aim to acknowledge reports within 72 hours and publish a patched release
within 14 days of a confirmed vulnerability.
