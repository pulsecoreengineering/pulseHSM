# Industrial Machine Controller

**Demonstrates:** safety-critical E-stop via superstate event bubbling, deep
hierarchy, `setInitial`, timed start-up sequence, `isInHierarchy()` for
real-time status output, `update()` for continuous monitoring.

## The problem

An industrial machine has a multi-step start-up, several operating sub-modes,
and a hard safety requirement: **any E-stop event must halt everything
immediately**, regardless of which sub-mode is active. A flat FSM duplicates the
E-stop handler in every state. An HSM puts it once in the `RUNNING` superstate.

## State diagram

```
MACHINE  (root superstate — E-stop handler here)
├── IDLE          (initial leaf; waiting for START command)
├── RUNNING       (superstate — all operational substates)
│   ├── STARTING  (initial; 5 s warm-up, then → OPERATING)
│   ├── OPERATING (superstate — normal production)
│   │   ├── NORMAL    (initial; steady-state)
│   │   └── BOOSTING  (temporary high-speed run; timeout → NORMAL)
│   └── PAUSED    (operator paused; awaiting RESUME)
└── FAULT         (E-stop or error; awaiting RESET)
```

**Events:**
- `EVT_START` — IDLE → RUNNING (→ STARTING via setInitial)
- `EVT_READY` — STARTING → OPERATING (→ NORMAL via setInitial)
- `EVT_BOOST` — NORMAL → BOOSTING
- `EVT_PAUSE` — any RUNNING child → PAUSED
- `EVT_RESUME` — PAUSED → OPERATING (→ NORMAL)
- `EVT_ESTOP` — **any state** → FAULT (handled at MACHINE superstate)
- `EVT_RESET` — FAULT → IDLE

## Full example

```cpp
#define PULSEHSM_MAX_STATES 16
#define PULSEHSM_MAX_EVENTS 16
#include "PulseHSM.h"

PulseHSM fsm;

// ---- State indices -------------------------------------------------------
int ST_MACHINE, ST_IDLE, ST_RUNNING, ST_STARTING;
int ST_OPERATING, ST_NORMAL, ST_BOOSTING, ST_PAUSED, ST_FAULT;

// ---- Events -------------------------------------------------------------
enum Events : uint8_t {
  EVT_START  = 1,
  EVT_READY,
  EVT_BOOST,
  EVT_PAUSE,
  EVT_RESUME,
  EVT_ESTOP,
  EVT_RESET,
};

// ---- Hardware stubs -----------------------------------------------------
void hw_spinUp()     { Serial.println("[HW] Spinning up…"); }
void hw_startProd()  { Serial.println("[HW] Production running."); }
void hw_boost()      { Serial.println("[HW] BOOST mode."); }
void hw_normalSpeed(){ Serial.println("[HW] Normal speed."); }
void hw_pause()      { Serial.println("[HW] Paused — drives held."); }
void hw_shutdown()   { Serial.println("[HW] EMERGENCY STOP — all drives off."); }
void hw_reset()      { Serial.println("[HW] Reset complete."); }

// ---- MACHINE (root superstate) -----------------------------------------
// E-stop fires here for ANY child, regardless of depth.
bool machineEvent(uint8_t e) {
  if (e == EVT_ESTOP) {
    hw_shutdown();
    fsm.transitionTo(ST_FAULT);
    return true;
  }
  return false;
}

// ---- IDLE ---------------------------------------------------------------
void idleEntry() { Serial.println("IDLE — send START."); }

bool idleEvent(uint8_t e) {
  if (e == EVT_START) {
    fsm.transitionTo(ST_RUNNING);   // → STARTING (initial substate)
    return true;
  }
  return false;
}

// ---- RUNNING (superstate) -----------------------------------------------
void running_entry() { Serial.println("RUNNING superstate entered."); }
void running_exit()  { Serial.println("RUNNING superstate exited."); }

bool running_event(uint8_t e) {
  if (e == EVT_PAUSE) {
    fsm.transitionTo(ST_PAUSED);
    return true;
  }
  return false;
}

// ---- STARTING -----------------------------------------------------------
void starting_entry() {
  Serial.println("STARTING — warm-up (5 s)…");
  hw_spinUp();
}

bool starting_event(uint8_t e) {
  if (e == EVT_READY) {
    fsm.transitionTo(ST_OPERATING);   // → NORMAL (initial substate)
    return true;
  }
  return false;
}

// ---- OPERATING (superstate) --------------------------------------------
void operating_entry() { hw_startProd(); }

bool operating_event(uint8_t e) {
  if (e == EVT_RESUME) {
    // Re-entering OPERATING lands in NORMAL (initial substate)
    // This event fires when coming back from PAUSED.
    fsm.transitionTo(ST_OPERATING);
    return true;
  }
  return false;
}

// ---- NORMAL -------------------------------------------------------------
void normal_entry() { hw_normalSpeed(); }

bool normal_event(uint8_t e) {
  if (e == EVT_BOOST) {
    fsm.transitionTo(ST_BOOSTING);
    return true;
  }
  return false;
}

// ---- BOOSTING -----------------------------------------------------------
//  Timeout of 10 s returns to NORMAL automatically
void boosting_entry()   { hw_boost(); }
void boosting_exit()    { hw_normalSpeed(); }

// ---- PAUSED -------------------------------------------------------------
void paused_entry() { hw_pause(); }

bool paused_event(uint8_t e) {
  if (e == EVT_RESUME) {
    // Going back to OPERATING re-enters at NORMAL (initial substate)
    fsm.transitionTo(ST_OPERATING);
    return true;
  }
  return false;
}

// ---- FAULT --------------------------------------------------------------
void fault_entry() {
  Serial.print("** FAULT ** — was in: ");
  Serial.println(fsm.getPreviousName());
  Serial.println("Send RESET to clear.");
}

bool fault_event(uint8_t e) {
  if (e == EVT_RESET) {
    hw_reset();
    fsm.transitionTo(ST_IDLE);
    return true;
  }
  return false;
}

// ---- Status display (runs every second, independent of state) -----------
void printStatus() {
  Serial.print("State: ");
  Serial.print(fsm.getCurrentName());
  if (fsm.isInHierarchy(ST_RUNNING))    Serial.print("  [RUNNING]");
  if (fsm.isInHierarchy(ST_OPERATING)) Serial.print("  [OPERATING]");
  Serial.print("  uptime in state: ");
  Serial.print(fsm.getStateElapsed() / 1000);
  Serial.println("s");
}

// ---- setup / loop -------------------------------------------------------
void setup() {
  Serial.begin(115200);

  // Root superstate first, then children in depth-first order
  ST_MACHINE   = fsm.addState("MACHINE",   nullptr, nullptr,        nullptr,       0,     -1, machineEvent,   -1);
  ST_IDLE      = fsm.addState("IDLE",      nullptr, idleEntry,      nullptr,       0,     -1, idleEvent,      ST_MACHINE);
  ST_FAULT     = fsm.addState("FAULT",     nullptr, fault_entry,    nullptr,       0,     -1, fault_event,    ST_MACHINE);
  ST_RUNNING   = fsm.addState("RUNNING",   nullptr, running_entry,  running_exit,  0,     -1, running_event,  ST_MACHINE);
  ST_STARTING  = fsm.addState("STARTING",  nullptr, starting_entry, nullptr,       5000,  -1, starting_event, ST_RUNNING);
  ST_PAUSED    = fsm.addState("PAUSED",    nullptr, paused_entry,   nullptr,       0,     -1, paused_event,   ST_RUNNING);
  ST_OPERATING = fsm.addState("OPERATING", nullptr, operating_entry,nullptr,       0,     -1, operating_event,ST_RUNNING);
  ST_NORMAL    = fsm.addState("NORMAL",    nullptr, normal_entry,   nullptr,       0,     -1, normal_event,   ST_OPERATING);
  ST_BOOSTING  = fsm.addState("BOOSTING",  nullptr, boosting_entry, boosting_exit, 10000, ST_NORMAL, nullptr, ST_OPERATING);

  // Wire up initial substates
  fsm.setInitial(ST_MACHINE,   ST_IDLE);
  fsm.setInitial(ST_RUNNING,   ST_STARTING);
  fsm.setInitial(ST_OPERATING, ST_NORMAL);

  fsm.begin(ST_MACHINE);   // → IDLE
}

void loop() {
  if (Serial.available()) {
    switch (Serial.read()) {
      case 's': fsm.sendEvent(EVT_START);   break;
      case 'r': fsm.sendEvent(EVT_READY);   break;
      case 'b': fsm.sendEvent(EVT_BOOST);   break;
      case 'p': fsm.sendEvent(EVT_PAUSE);   break;
      case 'g': fsm.sendEvent(EVT_RESUME);  break;
      case 'e': fsm.sendEvent(EVT_ESTOP);   break;
      case 'x': fsm.sendEvent(EVT_RESET);   break;
    }
  }

  static unsigned long lastStatus = 0;
  if (millis() - lastStatus >= 1000) { lastStatus = millis(); printStatus(); }

  fsm.update();
}
```

## Why this hierarchy works

**E-stop at the root**  
`machineEvent` is the `onEvent` of `ST_MACHINE`. Since every other state is a
descendant of `ST_MACHINE`, any unhandled `EVT_ESTOP` bubbles up to this handler.
You add one state, one handler, and the safety behaviour covers the entire machine.

**Nested initial substates**  
`begin(ST_MACHINE)` resolves: `ST_MACHINE` → `ST_IDLE` (leaf).  
`transitionTo(ST_RUNNING)` resolves: `ST_RUNNING` → `ST_STARTING` (leaf).  
`transitionTo(ST_OPERATING)` resolves: `ST_OPERATING` → `ST_NORMAL` (leaf).

**`STARTING` timeout with `EVT_READY` override**  
The warm-up timeout (`5000 ms`, `timeoutNext = -1`) is intentionally left without
a target (`-1`) — the real transition comes from an `EVT_READY` event sent when
the hardware confirms it is ready. Setting `timeoutNext = ST_OPERATING` instead
would give an automatic fallback after 5 s even without hardware confirmation.

**`isInHierarchy()` for the status line**  
The display code asks "is the machine currently running?" without caring which
specific sub-mode it is in. `isInHierarchy(ST_RUNNING)` returns `true` for
`STARTING`, `OPERATING`, `NORMAL`, `BOOSTING`, and `PAUSED` alike.
