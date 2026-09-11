# Changelog

## 1.2.0

### Added
- **Initial (default) substates** — `setInitial(parent, child)` marks a direct
  child as the default substate entered when a composite is targeted.
  `transitionTo(composite)` and `begin(composite)` now resolve recursively to
  the deepest initial leaf before running entry/exit chains. Fully backward
  compatible: without `setInitial`, every existing behaviour is unchanged.
- `sendEvent()` now returns `bool` (`true` = queued, `false` = dropped). Existing
  callers that ignore the return value are source-compatible.
- CI matrix extended to SAMD, RP2040, and STM32 — all architectures claimed in
  `library.properties` are now compiled on every push.
- Expanded test suite: initial-substate resolution, queue overflow (asserting the
  `false` return), multi-instance isolation, `isInHierarchy` across hierarchy
  levels, and reentrancy (`transitionTo` from inside `entry()`).

### Changed
- `begin()` accepts composite states that have an initial substate configured;
  it still rejects composites with no `setInitial` set.

## 1.1.0

### Added
- `PULSEHSM_SELF_TRANSITION_FULL_REINIT` compile-time option.
  - `0` (default): self-transitions reset the timer only (unchanged behaviour).
  - `1`: self-transitions run `exit()` then `entry()` for **that state only** —
    ancestors are left untouched (correct UML external self-transition semantics).
- Compile-time `static_assert`s for invalid configuration:
  - `PULSEHSM_MAX_EVENTS` must be a power of two (the ring buffer uses bitmask
    indexing; non-power-of-two sizes previously corrupted the queue **silently**).
  - `PULSEHSM_MAX_STATES` must be 1..127; `PULSEHSM_MAX_DEPTH` must be >= 1.
- Host-runnable regression test suite (`test/`) and a GitHub Actions workflow
  that runs it and compiles the examples for AVR and ESP32 on every push.

### Changed
- `sendEvent()` and the `update()` dequeue now run under a proper interrupt-safe
  critical section (save/restore on AVR and ARM Cortex-M). The previous
  "ISR-safe" claim was not backed by any guard. See the note in `PulseHSM.h`
  about the dual-core (ESP32/RP2040) cross-core caveat.
- `begin()` now returns `bool` and refuses an invalid or non-leaf start state
  instead of silently entering an unsupported composite state.
- `addState()` now returns `-1` if a state's depth would exceed
  `PULSEHSM_MAX_DEPTH`, instead of silently truncating the entry/exit chains.

