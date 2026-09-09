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
