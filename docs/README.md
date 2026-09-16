# PulseHSM

**Tiny hierarchical state machine for embedded systems.**  
No heap. No dynamic allocation. Interrupt-safe. Runs on AVR, ESP32, STM32, RP2040, SAMD.

---

## Why PulseHSM?

Flat state machines break down once you have more than a handful of states. You end up
copy-pasting the same E-stop handler into every state, or adding a sprawling `if/else`
chain that checks flags rather than following the machine's own structure.

A hierarchical state machine (HSM) lets shared behaviour live in a **superstate** — one
E-stop handler at the `RUNNING` level covers every substate automatically. PulseHSM brings
that model to microcontrollers with **zero heap use and a fully static footprint**.

```cpp
#include "PulseHSM.h"

// RUNNING is a superstate. Its E-stop handler fires for every substate.
// Entering RUNNING automatically lands in STARTING (its initial substate).

enum StateID : int8_t { ST_RUNNING = 0, ST_STARTING, ST_OPERATING, ST_FAULT, ST_COUNT };

void startEntry(); void opEntry(); void faultEntry();
bool onEstop(uint8_t e);

constexpr PulseHSM::StaticState TABLE[ST_COUNT] PULSEHSM_TABLE = {
    [ST_RUNNING]   = { PULSEHSM_NAME("RUNNING"),   nullptr, nullptr,    nullptr, 0, -1, onEstop,  -1,         ST_STARTING },
    [ST_STARTING]  = { PULSEHSM_NAME("STARTING"),  nullptr, startEntry, nullptr, 3000, -1, nullptr, ST_RUNNING, -1 },
    [ST_OPERATING] = { PULSEHSM_NAME("OPERATING"), nullptr, opEntry,    nullptr, 0, -1, nullptr, ST_RUNNING,   -1 },
    [ST_FAULT]     = { PULSEHSM_NAME("FAULT"),     nullptr, faultEntry, nullptr, 0, -1, nullptr, -1,           -1 },
};
PULSEHSM_VALIDATE_TABLE(TABLE, ST_COUNT);

PulseHSM fsm(TABLE, ST_COUNT);

void startEntry() { Serial.println("STARTING…"); }
void opEntry()    { Serial.println("OPERATING"); }
void faultEntry() { Serial.println("FAULT"); }
bool onEstop(uint8_t e) {
    if (e == 1) { fsm.transitionTo(ST_FAULT); return true; }
    return false;
}

void setup() { Serial.begin(115200); fsm.begin(ST_RUNNING); }  // → STARTING
void loop()  { fsm.update(); }
```

## Features at a glance

| Feature | Detail |
|---|---|
| Hierarchical states | `entry` / `exit` / `update` on superstates and leaves |
| Initial substates | `initialChild` field — target a composite and land in its default leaf |
| Event queue | Fixed ring buffer, interrupt-safe, optional `int32` payload |
| Event bubbling | Unhandled events walk up to the parent automatically |
| Timed transitions | `timeoutMs` + `timeoutNext` per state |
| Self-transitions | Lightweight (timer reset) or full reinit mode |
| Flash-resident tables | `PULSEHSM_TABLE` keeps the state table in AVR flash (PROGMEM) |
| Compile-time validation | `PULSEHSM_VALIDATE_TABLE` catches wiring errors before upload |
| Zero heap | Everything sized at compile time |

---

## Memory footprint

Defaults (`PULSEHSM_MAX_EVENTS 8`, `PULSEHSM_MAX_DEPTH 4`):

| MCU | Flash (approx.) | RAM (approx.) |
|---|---|---|
| AVR (ATmega328P) | ~1.1 KB | ~120 B |
| ARM Cortex-M (ESP32, RP2040, STM32, SAMD) | ~1.4 KB | ~160 B |

The `StaticState` table lives in flash on AVR (via `PULSEHSM_TABLE = PROGMEM`),
adding zero RAM overhead per state. Raising `PULSEHSM_MAX_EVENTS` adds 5 bytes
of RAM per slot.

---

## Limitations

- **State count**: up to 127 states per instance (indices are `int8_t`). The count
  is the element count `N` of your `StaticState TABLE[N]` — no macro needed.
- **Hierarchy depth**: `PULSEHSM_MAX_DEPTH` defaults to 4 ancestors per leaf. Override if needed.
- **Dual-core (RP2040, ESP32)**: `sendEvent()` is safe when called from an ISR **on
  the same core** as `update()`. Cross-core producers need additional user-side
  synchronisation (a mutex or memory barrier).
- **`millis()` rollover**: handled — timeout logic uses unsigned subtraction, surviving
  the 49-day rollover.
- **Single active leaf**: PulseHSM is a single-thread HSM. Orthogonal (parallel)
  regions are not supported.

---

## Get started

- [Concepts](guide/concepts.md) — what an HSM is and how PulseHSM models it
- [Quick Start](guide/quickstart.md) — from a two-state blinker to a full hierarchy in minutes
- [Initial Substates](guide/initial-substates.md) — the feature that makes a hierarchy feel like a hierarchy
- [API Reference](api/reference.md) — every method documented

## Use cases

Real-world patterns, fully worked out:

- [Vending Machine](use-cases/vending-machine.md)
- [Device Connection Manager](use-cases/device-manager.md)
- [Industrial Machine Controller](use-cases/machine-controller.md)
- [UI Menu System](use-cases/menu-system.md)
- [Serial Protocol Parser](use-cases/protocol-parser.md)
