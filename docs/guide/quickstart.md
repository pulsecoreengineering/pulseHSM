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

#ifndef LED_BUILTIN
  #define LED_BUILTIN 2   // ESP32 boards that don't define it
#endif

enum StateID : int8_t { ST_ON = 0, ST_OFF, ST_COUNT };

void onEntry()  { digitalWrite(LED_BUILTIN, HIGH); }
void offEntry() { digitalWrite(LED_BUILTIN, LOW);  }

//                         name            upd   entry     exit  ms    next   event  parent  init
constexpr PulseHSM::StaticState TABLE[ST_COUNT] PULSEHSM_TABLE = {
    [ST_ON]  = { PULSEHSM_NAME("ON"),  nullptr, onEntry,  nullptr, 500, ST_OFF, nullptr, -1, -1 },
    [ST_OFF] = { PULSEHSM_NAME("OFF"), nullptr, offEntry, nullptr, 500, ST_ON,  nullptr, -1, -1 },
};
PULSEHSM_VALIDATE_TABLE(TABLE, ST_COUNT);

PulseHSM fsm(TABLE, ST_COUNT);

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    fsm.begin(ST_ON);
}
void loop() { fsm.update(); }
```

State indices come from the `enum`. The table uses
[designated array initializers](https://gcc.gnu.org/onlinedocs/gcc/Designated-Inits.html)
(`[ST_ON] = {...}`) so the layout is self-documenting and order-independent.

## Step 2 — Add an event

Respond to a button press that toggles the blinker off permanently.

```cpp
enum Events : uint8_t { EVT_STOP = 1 };

bool onEvent(uint8_t e) {
    if (e == EVT_STOP) { fsm.transitionTo(ST_OFF); return true; }
    return false;
}

// Set onEvent as the onEvent field in the ST_ON row (sixth positional field after timeoutNext).
// From an ISR or loop():
//   fsm.sendEvent(EVT_STOP);
```

## Step 3 — Add a hierarchy

Group the two blink states under a `BLINKING` superstate, and add an `IDLE` state
for when blinking is stopped. The `BLINKING` superstate handles `EVT_STOP` once,
for both children.

```cpp
#include "PulseHSM.h"

#ifndef LED_BUILTIN
  #define LED_BUILTIN 2
#endif

enum StateID : int8_t { ST_BLINKING = 0, ST_ON, ST_OFF, ST_IDLE, ST_COUNT };
enum Events  : uint8_t { EVT_START = 1, EVT_STOP };

// Forward-declare callbacks (they reference fsm, defined after the table)
void onEntry();  void offEntry(); void idleEntry();
bool blinkingEvent(uint8_t e);   bool idleEvent(uint8_t e);

//                           name              upd   entry      exit  ms    next    event          parent      initChild
constexpr PulseHSM::StaticState TABLE[ST_COUNT] PULSEHSM_TABLE = {
    [ST_BLINKING] = { PULSEHSM_NAME("BLINKING"), nullptr, nullptr,   nullptr, 0,   -1,    blinkingEvent, -1,         ST_ON },
    [ST_ON]       = { PULSEHSM_NAME("ON"),       nullptr, onEntry,   nullptr, 500, ST_OFF, nullptr,      ST_BLINKING, -1   },
    [ST_OFF]      = { PULSEHSM_NAME("OFF"),      nullptr, offEntry,  nullptr, 500, ST_ON,  nullptr,      ST_BLINKING, -1   },
    [ST_IDLE]     = { PULSEHSM_NAME("IDLE"),     nullptr, idleEntry, nullptr, 0,   -1,    idleEvent,    -1,          -1   },
};
PULSEHSM_VALIDATE_TABLE(TABLE, ST_COUNT);

PulseHSM fsm(TABLE, ST_COUNT);

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
    fsm.begin(ST_BLINKING);   // initialChild = ST_ON → starts in ON
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
- `ST_BLINKING.initialChild = ST_ON` — `transitionTo(ST_BLINKING)` and
  `begin(ST_BLINKING)` both resolve to `ST_ON` automatically.
- `blinkingEvent` handles `EVT_STOP` once — both `ST_ON` and `ST_OFF` inherit it.
- Transitioning from `ST_ON` to `ST_OFF` exits `ST_ON` and enters `ST_OFF` only;
  `ST_BLINKING`'s `entry`/`exit` do **not** run (it is the LCA).
- Callbacks that reference `fsm` are forward-declared, then implemented **after**
  `fsm` is defined.

## What's next

- [Initial Substates](initial-substates.md) — `initialChild` field in depth
- [Events & Payloads](events.md) — `sendEvent()`, `getEventData()`, ISR safety
- [Use Cases](../use-cases/overview.md) — fully worked real-world examples
