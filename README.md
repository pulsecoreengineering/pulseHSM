![CI](https://img.shields.io/github/actions/workflow/status/pulsecoreengineering/pulseHSM/ci.yml?branch=main&label=CI)
![Stars](https://img.shields.io/github/stars/pulsecoreengineering/pulseHSM?style=flat)
![License](https://img.shields.io/github/license/pulsecoreengineering/pulseHSM)
![PlatformIO](https://img.shields.io/badge/PlatformIO-Registry-orange)
![Arduino](https://img.shields.io/badge/Arduino-Library_Manager-teal)
![Discussions](https://img.shields.io/github/discussions/pulsecoreengineering/pulseHSM)
# PulseHSM

A tiny hierarchical state machine (HSM) for embedded systems. No heap, no dynamic
allocation, bounded memory, interrupt-safe event queue. Runs on AVR, ESP32, STM32,
RP2040, SAMD and other Arduino-compatible targets.

## Features

- Hierarchical states with `entry` / `exit` / `update` actions
- Initial (default) substates — target a composite state and automatically enter its designated leaf
- Timed transitions (`timeoutMs` → `timeoutNext`)
- Fixed-size, interrupt-safe event queue with optional `int32` payload
- Event bubbling up the parent chain
- Deferred transitions (applied at the end of `update()`)
- Fully static: everything sized at compile time

## Concepts

### What is an HSM?

A hierarchical state machine is a finite state machine where states can contain
other states. A **composite** (or superstate) is a state that has children; a
**leaf** state has none. The machine is always *in* exactly one leaf state, but
it is simultaneously *in* all of that leaf's ancestors.

**Why does this matter?** Shared behaviour lives in superstates. An `onEvent`
handler in a superstate fires for every event that its children don't consume —
you write the E-stop logic once in `RUNNING` and it covers `STARTING`,
`PROCESSING`, and `STOPPING` automatically.

### Entry, exit, and update

| Callback | When it fires |
|---|---|
| `entry()` | Once when the machine enters the state (outer states first) |
| `exit()` | Once when the machine leaves the state (inner states first) |
| `update()` | Every `loop()` tick while the machine is in the state or any descendant |

When transitioning from one leaf to another, only the callbacks between the two
states' lowest common ancestor (LCA) are called. Shared superstates are never
exited or re-entered.

### Initial substates

A composite state can designate one of its direct children as its **initial
substate** — the default child entered when the composite is targeted:

```cpp
// P -> { A, B };  A is the initial substate of P
fsm.setInitial(P, A);
fsm.transitionTo(P);   // lands in A, running P's entry then A's entry
```

Resolution is recursive: if A is also a composite with its own initial child,
the machine descends all the way to the deepest initial leaf.

### Events

Events are enqueued with `sendEvent()` (interrupt-safe) and dispatched one per
`update()` tick. The machine walks from the current leaf up through its ancestors
calling each state's `onEvent` until one returns `true` (handled). An `int32`
payload can accompany every event; read it with `getEventData()` inside the
handler.

### Self-transitions

`transitionTo(currentState)` is a self-transition. In the default *lightweight*
mode (`PULSEHSM_SELF_TRANSITION_FULL_REINIT=0`) only the timer resets — no
`entry`/`exit` runs. In *full-reinit* mode (`=1`) `exit()` then `entry()` run
for that one state; ancestors are untouched.

## Configure (before `#include "PulseHSM.h"`, or via `-D`)

| Macro | Default | Notes |
|---|---|---|
| `PULSEHSM_MAX_STATES` | 8 | 1..127 |
| `PULSEHSM_MAX_EVENTS` | 8 | **must be a power of two** |
| `PULSEHSM_MAX_DEPTH` | 4 | max ancestors per leaf |
| `PULSEHSM_SELF_TRANSITION_FULL_REINIT` | 0 | 0 = timer reset only; 1 = exit()+entry() for that state |

Invalid values fail at compile time with a clear message.

## API reference

```cpp
// ---- Setup ------------------------------------------------------------------

// Add a state. Returns its index (0, 1, 2, …) or -1 on error.
// parent = -1 for a root state. A parent must be added before its children.
// Returns -1 if the table is full or depth would exceed PULSEHSM_MAX_DEPTH.
int addState(const char* name,
             Action      update,       // called every tick (may be nullptr)
             Action      entry,        // called on entry (may be nullptr)
             Action      exit,         // called on exit (may be nullptr)
             unsigned long timeoutMs,  // 0 = no timeout
             int         timeoutNext,  // state to go to on timeout (-1 = none)
             EventCb     onEvent,      // event handler (may be nullptr)
             int         parent = -1); // parent state index

// Mark `child` as the default substate of `parent`.
// child must be a direct child of parent. Returns false on bad indices.
bool setInitial(int parent, int child);

// Start the machine. startState may be a leaf or a composite with setInitial set.
// Calls the full entry chain from root to startState (recursing into initial substates).
// Returns false if startState is invalid or is a composite with no initial substate.
bool begin(int startState);

// ---- Main loop --------------------------------------------------------------

// Call once per loop(). Drains the event queue, runs update() callbacks,
// checks timeouts, and applies any pending transition.
void update();

// ---- Transitions & events ---------------------------------------------------

// Request a transition. Applied at the end of the current update().
// toState may be a leaf or a composite with setInitial configured.
void transitionTo(int toState);

// Enqueue an event (interrupt-safe). Returns true if queued, false if full.
bool sendEvent(uint8_t event, int32_t data = 0);

// ---- Queries ----------------------------------------------------------------

int          getCurrentState()  const;  // current leaf state index
const char*  getCurrentName()   const;  // current state's name string
const char*  getStateName(int)  const;  // name of any state by index
unsigned long getStateElapsed() const;  // ms since last entry
int          getPreviousState() const;  // state before last transition (-1 = none)
const char*  getPreviousName()  const;
int32_t      getEventData()     const;  // payload of the event being dispatched
bool         isInHierarchy(int state) const; // true if state is current or an active ancestor
```

**Callback signatures:**

```cpp
using Action  = void (*)();
using EventCb = bool (*)(uint8_t event);  // return true = handled, false = bubble up
```

## Examples

Each example is self-contained and compiled for AVR, ESP32, SAMD, RP2040, and STM32 in CI.

| Example | Shows |
|---|---|
| `BlinkHSM` | Basics: leaf states, timed transitions, one event |
| `TrafficLight` | Flat timed FSM; an event pre-empting a timed transition |
| `ButtonDebounce` | Polling in `update()` with timed settling states |
| `MachineControl` | **Hierarchy**: a superstate handling E-stop once for all substates |
| `Thermostat` | Event payloads via `sendEvent(evt, data)` / `getEventData()` |
| `ConnectionRetry` | Self-transitions and `PULSEHSM_SELF_TRANSITION_FULL_REINIT` (both modes) |
| `InterruptButton` | Injecting events from an ISR into the interrupt-safe queue |

Start with `MachineControl` if you want to see why *hierarchical* matters.

## Quick start

A two-state blinker (see [`examples/BlinkHSM`](examples/BlinkHSM/BlinkHSM.ino)):

```cpp
#include "PulseHSM.h"
enum { ST_ON, ST_OFF };
PulseHSM fsm;

void setup() {
  fsm.addState("on",  nullptr, onEntry,  nullptr, 500, ST_OFF, nullptr, -1);
  fsm.addState("off", nullptr, offEntry, nullptr, 500, ST_ON,  nullptr, -1);
  fsm.begin(ST_ON);
}
void loop() { fsm.update(); }
```

A hierarchy with an initial substate:

```cpp
#include "PulseHSM.h"
enum { RUNNING, STARTING, PROCESSING };  // RUNNING is composite
PulseHSM fsm;

void setup() {
  fsm.addState("RUNNING",    nullptr, nullptr, nullptr, 0, -1, runningEvent, -1);
  fsm.addState("STARTING",   nullptr, startEntry, nullptr, 2000, PROCESSING, nullptr, RUNNING);
  fsm.addState("PROCESSING", nullptr, procEntry,  nullptr, 0,    -1,         nullptr, RUNNING);
  fsm.setInitial(RUNNING, STARTING);  // entering RUNNING lands in STARTING
  fsm.begin(RUNNING);                 // resolves to STARTING
}
void loop() { fsm.update(); }
```

## Tests

`test/` holds a host-buildable regression suite (no board needed):

```bash
bash test/run_tests.sh
```

CI (`.github/workflows/ci.yml`) runs those tests in both self-transition modes
and compiles every example for AVR, ESP32, SAMD, RP2040, and STM32 on each push.

## Interrupt safety

`sendEvent()` is safe to call from an ISR. On AVR and ARM Cortex-M the queue is
guarded with a save/restore critical section. On dual-core parts (ESP32, RP2040)
it is safe from ISRs and tasks on the **same core**; a producer on the other core
needs its own synchronisation.
