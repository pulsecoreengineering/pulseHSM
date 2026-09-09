# Changelog

All notable changes to PulseHSM are documented here.

---

## v1.2.0 — 2025-07-15

**First public release.**

### Features
- Hierarchical state machine (HSM) with unlimited nesting up to `PULSEHSM_MAX_DEPTH`.
- `entry` / `exit` / `update` callbacks on every state.
- `setInitial(parent, child)` — automatic resolution to the deepest initial leaf on `begin()` and `transitionTo()`.
- Fixed-size interrupt-safe ring-buffer event queue with optional `int32_t` payload per event.
- Event bubbling — unhandled events walk up the ancestor chain automatically.
- Per-state timed transitions (`timeoutMs` + `timeoutNext`).
- Self-transition: lightweight (timer reset only) or full reinit (`PULSEHSM_SELF_TRANSITION_FULL_REINIT`).
- `isInHierarchy()` — query whether a state is active at any level.
- `getPreviousState()` / `getPreviousName()` — inspect the prior state from inside callbacks.
- `getStateElapsed()` — milliseconds since the current state was entered.
- `getEventData()` — `int32_t` payload of the event being dispatched.
- Interrupt-safe critical section: AVR (SREG save/restore), RP2040 (Pico SDK), ARM Cortex-M (PRIMASK), fallback (`noInterrupts`/`interrupts`).
- Compile-time sanity checks (`static_assert`) on configuration values.

### Supported targets
AVR (ATmega328P, ATmega2560), ESP32, RP2040, STM32, SAMD21/51, nRF52, Teensy, and any Arduino-compatible board.

### Configuration defaults

| Macro | Default | Meaning |
|---|---|---|
| `PULSEHSM_MAX_STATES` | 8 | Maximum number of states (max 127) |
| `PULSEHSM_MAX_EVENTS` | 8 | Event queue capacity (must be a power of two) |
| `PULSEHSM_MAX_DEPTH` | 4 | Maximum ancestor depth per leaf |
| `PULSEHSM_SELF_TRANSITION_FULL_REINIT` | 0 | Self-transition mode (0 = lightweight, 1 = full reinit) |

---

## Known issues

### v1.2.0

| # | Severity | Description | Workaround |
|---|---|---|---|
| 1 | Low | Cross-core `sendEvent()` on RP2040 / ESP32 is not thread-safe without extra locking. | Wrap `sendEvent()` in a platform mutex when calling from a different core or task. |
| 2 | Info | `<Arduino.h>` is required — no bare-metal (non-Arduino) port yet. | Provide `millis()`, `noInterrupts()`, and `interrupts()` as stubs; the rest compiles. |
| 3 | Info | Orthogonal (parallel) regions are not supported. | Run two separate `PulseHSM` instances and share variables or events between them. |

To report a new issue or request a fix: [open a GitHub issue](https://github.com/pulsecoreengineering/pulseHSM/issues).
