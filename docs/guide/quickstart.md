# Quick Start

## Installation

1. Download or clone the repository.
2. Copy (or symlink) the folder into your Arduino `libraries/` directory as `PulseHSM`.
3. Restart the Arduino IDE.

```
~/Arduino/libraries/PulseHSM/
  PulseHSM.h
  PulseHSM.cpp
  examples/
  ...
```

## Step 1 — Two leaf states (a blinker)

The simplest possible machine: two states with timed transitions between them.

```cpp
#include "PulseHSM.h"

PulseHSM fsm;
enum { ST_ON, ST_OFF };   // indices match addState() call order

void onEntry()  { digitalWrite(LED_BUILTIN, HIGH); }
void offEntry() { digitalWrite(LED_BUILTIN, LOW);  }

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  //         name     upd   entry     exit  timeout  next    event  parent
  fsm.addState("on",  nullptr, onEntry,  nullptr, 500, ST_OFF, nullptr, -1);
  fsm.addState("off", nullptr, offEntry, nullptr, 500, ST_ON,  nullptr, -1);
  fsm.begin(ST_ON);
}
void loop() { fsm.update(); }
```

`addState()` returns the state's index in the order it was added, so you can
pre-declare an enum that matches that order, or capture the return value:

```cpp
int ST_ON  = fsm.addState("on",  ...);   // 0
int ST_OFF = fsm.addState("off", ...);   // 1
```

## Step 2 — Add an event

Respond to a button press that toggles the blinker off permanently.

```cpp
enum Events { EVT_STOP = 1 };

bool onEvent(uint8_t e) {
  if (e == EVT_STOP) { fsm.transitionTo(ST_OFF); return true; }
  return false;
}

// In setup(), add onEvent to both states' addState() call (or to a superstate).
// From an ISR or loop():
//   fsm.sendEvent(EVT_STOP);
```

## Step 3 — Add a hierarchy

Group the two blink states under a `BLINKING` superstate, and add an `IDLE` state
for when blinking is stopped. The `BLINKING` superstate handles `EVT_STOP` once,
for both children.

```cpp
#include "PulseHSM.h"

PulseHSM fsm;
enum Events { EVT_START = 1, EVT_STOP };

int ST_BLINKING, ST_ON, ST_OFF, ST_IDLE;

void onEntry()  { digitalWrite(LED_BUILTIN, HIGH); }
void offEntry() { digitalWrite(LED_BUILTIN, LOW);  }
void idleEntry(){ digitalWrite(LED_BUILTIN, LOW);  }

bool blinkingEvent(uint8_t e) {
  if (e == EVT_STOP)  { fsm.transitionTo(ST_IDLE);     return true; }
  return false;
}
bool idleEvent(uint8_t e) {
  if (e == EVT_START) { fsm.transitionTo(ST_BLINKING); return true; }
  return false;
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  // Parent before children
  ST_BLINKING = fsm.addState("BLINKING", nullptr, nullptr,  nullptr, 0,   -1,      blinkingEvent, -1);
  ST_IDLE     = fsm.addState("IDLE",     nullptr, idleEntry,nullptr, 0,   -1,      idleEvent,     -1);
  ST_ON       = fsm.addState("ON",       nullptr, onEntry,  nullptr, 500, ST_OFF,  nullptr,       ST_BLINKING);
  ST_OFF      = fsm.addState("OFF",      nullptr, offEntry, nullptr, 500, ST_ON,   nullptr,       ST_BLINKING);

  fsm.setInitial(ST_BLINKING, ST_ON);   // entering BLINKING lands in ON
  fsm.begin(ST_BLINKING);               // starts in ON
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 's') fsm.sendEvent(EVT_STOP);
    if (c == 'g') fsm.sendEvent(EVT_START);
  }
  fsm.update();
}
```

Key points:
- `ST_BLINKING` is added **before** `ST_ON` and `ST_OFF` because it is their parent.
- `setInitial(ST_BLINKING, ST_ON)` means `transitionTo(ST_BLINKING)` and
  `begin(ST_BLINKING)` both resolve to `ST_ON`.
- `blinkingEvent` handles `EVT_STOP` once — both `ST_ON` and `ST_OFF` inherit it.
- Transitioning from `ST_ON` to `ST_OFF` exits `ST_ON` and enters `ST_OFF` only;
  `ST_BLINKING`'s `entry`/`exit` do **not** run (it is the LCA).

## What's next

- [Initial Substates](initial-substates.md) — `setInitial()` in depth
- [Events & Payloads](events.md) — `sendEvent()`, `getEventData()`, ISR safety
- [Use Cases](../use-cases/overview.md) — fully worked real-world examples
