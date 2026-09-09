# Contributing to PulseHSM

Thank you for taking the time to contribute. PulseHSM is a small, focused
library and contributions that keep it lean and embedded-friendly are most
welcome.

## Quick start

1. Fork the repository and create a branch from `main`.
2. Make your change.
3. Add or update tests in `test/test_pulsehsm.cpp` to cover your change.
4. Run the test suite locally: `cd test && make && ./test_runner` (requires
   a C++11 compiler; no Arduino hardware needed).
5. Open a pull request with a short description of the problem and the fix.

## What we welcome

- Bug fixes with a failing test that becomes passing.
- Performance improvements that do not grow RAM/flash footprint on AVR.
- Documentation corrections (typos, broken links, wrong API signatures).
- New use-case examples in `docs/use-cases/`.
- CI improvements (new boards, faster build times).

## What to avoid

- New heap allocations (`new`, `malloc`, `std::vector`).
- Dependencies beyond the Arduino core.
- Features that only make sense on 32-bit targets (unless guarded by a macro
  and backward-compatible on AVR).
- Unrelated style clean-ups mixed into a functional change.

## Code style

- Match the style of the surrounding code.
- No dynamic allocation.
- No C++ exceptions or RTTI.
- Keep callback types as plain function pointers (`Action`, `EventCb`).
- Prefer `uint8_t` / `int8_t` for indices stored in the state table.

## Tests

The test suite lives in `test/test_pulsehsm.cpp` and compiles with a plain
`g++` Makefile — no Arduino toolchain required. Each test function is named
`test_<what_it_checks>()`. Add your test there before the final
`return tests_failed;` line and call it from `main()`.

## Commit messages

Use a short imperative subject line (≤ 72 chars), e.g.:

```
fix: initialChild defaults to -1 instead of 0
feat: add setInitial() for composite state resolution
docs: add Serial Protocol Parser use case
```

## Opening an issue

Before opening a new issue, please search existing ones. When reporting a
bug, include:

- PulseHSM version (from `library.properties`).
- Target board / architecture.
- Minimal sketch that reproduces the problem.
- Expected vs. actual behaviour.

## License

By contributing you agree that your contributions will be licensed under the
same MIT licence as the rest of the project.
