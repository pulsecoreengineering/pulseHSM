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
substate** — the default child entered when the composite is targeted. Set the
`initialChild` field in the composite's `StaticState` row:

```cpp
// P -> { A, B };  A is the initial substate of P — set in the table:
// [ST_P] = { ..., /*initialChild=*/ ST_A }
fsm.transitionTo(ST_P);   // lands in A, running P's entry then A's entry
```

Resolution is recursive: if A is also a composite with its own `initialChild`,
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
| `PULSEHSM_MAX_EVENTS` | 8 | **must be a power of two** |
| `PULSEHSM_MAX_DEPTH` | 4 | max ancestors per leaf |
| `PULSEHSM_SELF_TRANSITION_FULL_REINIT` | 0 | 0 = timer reset only; 1 = exit()+entry() for that state |
| `PULSEHSM_NAMES` | 1 | set to `0` to strip all name strings from the binary |

State count is determined by the element count of your `StaticState` table — no
`PULSEHSM_MAX_STATES` macro. Invalid values fail at compile time with a clear message.

## API reference

**Define your table, validate it, then construct:**

```cpp
enum StateID : int8_t { ST_A = 0, ST_B, ST_COUNT };

constexpr PulseHSM::StaticState TABLE[ST_COUNT] PULSEHSM_TABLE = {
    //       name                    update  entry   exit  ms  next   event  parent  initialChild
    [ST_A] = { PULSEHSM_NAME("A"), nullptr, entryA, nullptr, 0,  -1, nullptr, -1, -1 },
    [ST_B] = { PULSEHSM_NAME("B"), nullptr, entryB, nullptr, 0,  -1, nullptr, -1, -1 },
};
PULSEHSM_VALIDATE_TABLE(TABLE, ST_COUNT);
PulseHSM fsm(TABLE, ST_COUNT);
```

**Runtime methods:**

```cpp
// ---- Setup ------------------------------------------------------------------

// Start the machine. startState may be a leaf or a composite with initialChild set.
// Calls the full entry chain. Returns false if startState is invalid.
bool begin(int startState);

// ---- Main loop --------------------------------------------------------------

void update();  // call once per loop()

// ---- Transitions & events ---------------------------------------------------

// Deferred transition — applied at the end of the current update().
void transitionTo(int toState);

// Enqueue an event (interrupt-safe). Returns true if queued, false if full.
bool sendEvent(uint8_t event, int32_t data = 0);

// ---- Queries ----------------------------------------------------------------

int           getCurrentState()   const;  // current leaf state index
const char*   getCurrentName()    const;  // current state's name string
const char*   getStateName(int)   const;  // name of any state by index
unsigned long getStateElapsed()   const;  // ms since last entry
int           getPreviousState()  const;  // state before last transition (-1 = none)
const char*   getPreviousName()   const;
int32_t       getEventData()      const;  // payload of the event being dispatched
uint8_t       getDroppedEvents()  const;  // sendEvent() calls dropped since begin()
bool          isInHierarchy(int)  const;  // true if state is current or an active ancestor
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

A two-state blinker (see [`examples/Blink`](examples/Blink/Blink.ino)):

```cpp
#include "PulseHSM.h"

enum StateID : int8_t { ST_ON = 0, ST_OFF, ST_COUNT };

void onEntry()  { digitalWrite(LED_BUILTIN, HIGH); }
void offEntry() { digitalWrite(LED_BUILTIN, LOW);  }

constexpr PulseHSM::StaticState TABLE[ST_COUNT] PULSEHSM_TABLE = {
    [ST_ON]  = { PULSEHSM_NAME("ON"),  nullptr, onEntry,  nullptr, 500, ST_OFF, nullptr, -1, -1 },
    [ST_OFF] = { PULSEHSM_NAME("OFF"), nullptr, offEntry, nullptr, 500, ST_ON,  nullptr, -1, -1 },
};
PULSEHSM_VALIDATE_TABLE(TABLE, ST_COUNT);
PulseHSM fsm(TABLE, ST_COUNT);

void setup() { pinMode(LED_BUILTIN, OUTPUT); fsm.begin(ST_ON); }
void loop()  { fsm.update(); }
```

A hierarchy with an initial substate:

```cpp
#include "PulseHSM.h"

enum StateID : int8_t { ST_RUNNING = 0, ST_STARTING, ST_PROCESSING, ST_COUNT };

void startEntry(); void procEntry(); bool runningEvent(uint8_t e);

constexpr PulseHSM::StaticState TABLE[ST_COUNT] PULSEHSM_TABLE = {
    [ST_RUNNING]    = { PULSEHSM_NAME("RUNNING"),    nullptr, nullptr,    nullptr, 0,    -1,            runningEvent, -1,         ST_STARTING },
    [ST_STARTING]   = { PULSEHSM_NAME("STARTING"),   nullptr, startEntry, nullptr, 2000, ST_PROCESSING, nullptr,      ST_RUNNING, -1          },
    [ST_PROCESSING] = { PULSEHSM_NAME("PROCESSING"), nullptr, procEntry,  nullptr, 0,    -1,            nullptr,      ST_RUNNING, -1          },
};
PULSEHSM_VALIDATE_TABLE(TABLE, ST_COUNT);
PulseHSM fsm(TABLE, ST_COUNT);

// callback implementations follow (after fsm is defined) …

void setup() { fsm.begin(ST_RUNNING); }  // initialChild → STARTING
void loop()  { fsm.update(); }
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
