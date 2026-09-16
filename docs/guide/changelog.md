# Changelog

All notable changes to PulseHSM are documented here.

---

## v2.0.0 — Breaking API change

### Changed (breaking)
- **State tables are now compile-time constants.** The dynamic `addState()` and
  `setInitial()` methods are removed. Define your state machine as a
  `constexpr StaticState[]` array marked `PULSEHSM_TABLE`, validate it with
  `PULSEHSM_VALIDATE_TABLE(table, count)`, and pass the table to the new
  constructor: `PulseHSM fsm(TABLE, STATE_COUNT)`.
- `PULSEHSM_MAX_STATES` is removed. The element count of the table replaces it.
- The `initialChild` field in `StaticState` replaces `setInitial()`.

### Added
- `PULSEHSM_TABLE` macro: empty on flash-mapped targets (ESP32, Cortex-M,
  RP2040); expands to `PROGMEM` on AVR, keeping the table in flash instead of
  SRAM at no extra code cost.
- `PULSEHSM_RD_I8 / _U32 / _PTR` field-reader macros abstract `pgm_read_*` on
  AVR and plain pointer dereferences everywhere else.
- `PULSEHSM_NAMES` compile flag: set to `0` to strip every state-name string
  from the binary (useful on 2 KB AVR parts).
- `getDroppedEvents()` — returns the number of `sendEvent()` calls that were
  rejected because the queue was full; saturates at 255.
- Hot-path cache (`_updateChain[]`, `_curTimeoutMs`, `_curTimeoutNext`) rebuilt
  once per transition; avoids re-walking the parent chain or re-reading flash on
  every `loop()`.
- `PULSEHSM_VALIDATE_TABLE` catches parent/child errors, depth violations, cycles,
  composites missing an `initialChild`, and half-wired timeouts as **compiler
  errors** — nothing that used to be a runtime hang survives to the MCU.

### Migration from 1.x

Replace dynamic construction with a table. For example, a two-state blink:

```cpp
// 1.x
PulseHSM fsm;
fsm.addState("LED_ON",  nullptr, ledOn,  nullptr, 500, 1, nullptr);
fsm.addState("LED_OFF", nullptr, ledOff, nullptr, 500, 0, nullptr);
fsm.begin(0);

// 2.0
enum StateID : int8_t { ST_LED_ON=0, ST_LED_OFF, ST_COUNT };
constexpr PulseHSM::StaticState TABLE[ST_COUNT] PULSEHSM_TABLE = {
    [ST_LED_ON]  = {PULSEHSM_NAME("LED_ON"),  nullptr, ledOn,  nullptr, 500, ST_LED_OFF, nullptr, -1, -1},
    [ST_LED_OFF] = {PULSEHSM_NAME("LED_OFF"), nullptr, ledOff, nullptr, 500, ST_LED_ON,  nullptr, -1, -1},
};
PULSEHSM_VALIDATE_TABLE(TABLE, ST_COUNT);
PulseHSM fsm(TABLE, ST_COUNT);
fsm.begin(ST_LED_ON);
```

---

## v1.2.0 — 2025-07-15

**First public release** (superseded by v2.0.0).

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

---

## Known issues

| # | Severity | Description | Workaround |
|---|---|---|---|
| 1 | Low | Cross-core `sendEvent()` on RP2040 / ESP32 is not thread-safe without extra locking. | Wrap `sendEvent()` in a platform mutex when calling from a different core or task. |
| 2 | Info | `<Arduino.h>` is required — no bare-metal (non-Arduino) port yet. | Provide `millis()`, `noInterrupts()`, and `interrupts()` as stubs; the rest compiles. |
| 3 | Info | Orthogonal (parallel) regions are not supported. | Run two separate `PulseHSM` instances and share variables or events between them. |

To report a new issue or request a fix: [open a GitHub issue](https://github.com/pulsecoreengineering/pulseHSM/issues).
