# Changelog

## 2.0.0 — Breaking API change

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
  SRAM at no extra code.
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
    [ST_LED_ON]  = {PULSEHSM_NAME("LED_ON"),  nullptr,ledOn, nullptr,500,ST_LED_OFF,nullptr,-1,-1},
    [ST_LED_OFF] = {PULSEHSM_NAME("LED_OFF"), nullptr,ledOff,nullptr,500,ST_LED_ON, nullptr,-1,-1},
};
PULSEHSM_VALIDATE_TABLE(TABLE, ST_COUNT);
PulseHSM fsm(TABLE, ST_COUNT);
fsm.begin(ST_LED_ON);
```

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

