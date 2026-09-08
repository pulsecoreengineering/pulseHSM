# PulseHSM

A tiny hierarchical state machine (HSM) for embedded systems. No heap, no dynamic
allocation, bounded memory, interrupt-safe event queue. Runs on AVR, ESP32, STM32,
RP2040, SAMD and other Arduino-compatible targets.

## Features

- Hierarchical states with `entry` / `exit` / `update` actions
- Timed transitions (`timeoutMs` → `timeoutNext`)
- Fixed-size, interrupt-safe event queue with optional `int32` payload
- Event bubbling up the parent chain
- Deferred transitions (applied at the end of `update()`)
- Fully static: everything sized at compile time

## Configure (before `#include "PulseHSM.h"`, or via `-D`)

| Macro | Default | Notes |
|---|---|---|
| `PULSEHSM_MAX_STATES` | 8 | 1..127 |
| `PULSEHSM_MAX_EVENTS` | 8 | **must be a power of two** |
| `PULSEHSM_MAX_DEPTH` | 4 | max ancestors per leaf |
| `PULSEHSM_SELF_TRANSITION_FULL_REINIT` | 0 | 0 = timer reset only; 1 = exit()+entry() for that state only |

Invalid values now fail at compile time with a clear message instead of
misbehaving silently.

## Examples

Each example is self-contained and compiles for AVR and ESP32 in CI.

| Example | Shows |
|---|---|
| `BlinkHSM` | Basics: leaf states, timed transitions, one event |
| `TrafficLight` | Flat timed FSM; an event pre-empting a timed transition |
| `ButtonDebounce` | Polling in `update()` with timed settling states |
| `MachineControl` | **Hierarchy**: a superstate handling E-stop once for all substates (event bubbling + entry/exit chaining) |
| `Thermostat` | Event payloads via `sendEvent(evt, data)` / `getEventData()` |
| `ConnectionRetry` | Self-transitions and `PULSEHSM_SELF_TRANSITION_FULL_REINIT` (both modes) |
| `InterruptButton` | Injecting events from an ISR into the interrupt-safe queue |

Start with `MachineControl` if you want to see why *hierarchical* matters.

## Quick start

See [`examples/BlinkHSM`](examples/BlinkHSM/BlinkHSM.ino).

```cpp
#include "PulseHSM.h"
enum { ST_ON, ST_OFF };            // indices match addState() call order
PulseHSM fsm;

void onEntry()  { /* ... */ }
void offEntry() { /* ... */ }

void setup() {
  fsm.addState("on",  nullptr, onEntry,  nullptr, 500, ST_OFF, nullptr, -1);
  fsm.addState("off", nullptr, offEntry, nullptr, 500, ST_ON,  nullptr, -1);
  fsm.begin(ST_ON);                // returns false if the state isn't a valid leaf
}
void loop() { fsm.update(); }
```

## Tests

`test/` holds a host-buildable regression suite (no board needed):

```bash
bash test/run_tests.sh
```

CI (`.github/workflows/ci.yml`) runs those tests in both self-transition modes
and compiles every example for AVR and ESP32 on each push.

## Known limitation

Entering a composite (non-leaf) state is unsupported — there is no
default/initial-substate mechanism, so `begin()` and `transitionTo()` should
target leaf states. `begin()` returns `false` if given a non-leaf.

## Interrupt safety

`sendEvent()` is safe to call from an ISR. On AVR and ARM Cortex-M the queue is
guarded with a save/restore critical section. On dual-core parts (ESP32, RP2040)
it is safe from ISRs and tasks on the **same core**; a producer on the other core
needs its own synchronisation.
