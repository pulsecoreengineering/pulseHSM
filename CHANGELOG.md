# Changelog

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

### Notes
- Entering a composite (non-leaf) state is still unsupported: PulseHSM has no
  default/initial-substate mechanism. This is a known limitation, not a bug.
