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
PulseHSM fsm;

// RUNNING is a superstate. Its E-stop handler fires for every substate.
// Entering RUNNING automatically lands in STARTING (its initial substate).
void setup() {
  int RUNNING  = fsm.addState("RUNNING",  nullptr, nullptr, nullptr, 0, -1, onEstop,   -1);
  int STARTING = fsm.addState("STARTING", nullptr, startEntry, nullptr, 3000, -1, nullptr, RUNNING);
  int OPERATING= fsm.addState("OPERATING",nullptr, opEntry,    nullptr, 0,    -1, nullptr, RUNNING);
  int FAULT    = fsm.addState("FAULT",    nullptr, faultEntry, nullptr, 0,    -1, nullptr, -1);

  fsm.setInitial(RUNNING, STARTING);    // entering RUNNING lands in STARTING
  fsm.begin(RUNNING);                   // → STARTING
}
void loop() { fsm.update(); }
```

## Features at a glance

| Feature | Detail |
|---|---|
| Hierarchical states | `entry` / `exit` / `update` on superstates and leaves |
| Initial substates | `setInitial(parent, child)` — target a composite and land in its default leaf |
| Event queue | Fixed ring buffer, interrupt-safe, optional `int32` payload |
| Event bubbling | Unhandled events walk up to the parent automatically |
| Timed transitions | `timeoutMs` + `timeoutNext` per state |
| Self-transitions | Lightweight (timer reset) or full reinit mode |
| Zero heap | Everything sized at compile time with `#define` overrides |

---

## Memory footprint

Defaults (`PULSEHSM_MAX_STATES 8`, `PULSEHSM_MAX_EVENTS 8`, `PULSEHSM_MAX_DEPTH 4`):

| MCU | Flash (approx.) | RAM (approx.) |
|---|---|---|
| AVR (ATmega328P) | ~1.1 KB | ~160 B |
| ARM Cortex-M (ESP32, RP2040, STM32, SAMD) | ~1.4 KB | ~200 B |

Each additional state adds ~16 bytes of RAM. Raising `PULSEHSM_MAX_EVENTS` adds 5 bytes per slot.

---

## Limitations

- **State count**: `PULSEHSM_MAX_STATES` defaults to 8 (max 127). Raise it with a `#define` before the include.
- **Hierarchy depth**: `PULSEHSM_MAX_DEPTH` defaults to 4 ancestors per leaf. Deeper nesting requires overriding.
- **Dual-core (RP2040, ESP32)**: `sendEvent()` is safe when called from an ISR **on the same core** as `update()`. Cross-core producers need additional user-side synchronisation (a mutex or memory barrier).
- **`millis()` rollover**: handled — `getStateElapsed()` and timeout logic use subtraction, so they survive the 49-day rollover correctly.
- **No dynamic state removal**: states are registered once in `setup()` and exist for the lifetime of the program. This is intentional — it keeps the footprint static.
- **Single active leaf**: PulseHSM is a single-thread HSM. It does not support orthogonal (parallel) regions.

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
