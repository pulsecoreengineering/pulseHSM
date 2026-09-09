# Vending Machine

**Demonstrates:** hierarchy, `setInitial`, event payloads, shared fault handling
via event bubbling, `isInHierarchy()` for display logic.

## State diagram

```
OPERATIONAL  ← EVT_FAULT → OUT_OF_SERVICE (bubbles from any child)
├── IDLE              (initial; waiting for first coin)
├── HAS_MONEY         (coins inserted)
│   ├── SELECTING     (initial; waiting for product selection)
│   └── CONFIRMED     (item selected and funds verified)
└── DISPENSING        (motor running; waiting for done signal)

OUT_OF_SERVICE  ← EVT_SERVICED → OPERATIONAL
```

## What PulseHSM makes easy

- `EVT_FAULT` is handled **once** in `OPERATIONAL`. Whether the machine is idle,
  mid-selection, or dispensing, the fault handler fires automatically via bubbling.
- `transitionTo(ST_HAS_MONEY)` always lands in `SELECTING` (the initial substate)
  — no need to spell it out at every call site.
- `isInHierarchy(ST_OPERATIONAL)` lets the display code know whether to show the
  normal UI or the out-of-service banner without reaching into the machine's
  internals.

## Full example

```cpp
#define PULSEHSM_MAX_STATES 16
#define PULSEHSM_MAX_EVENTS 16
#include "PulseHSM.h"

PulseHSM fsm;

// ---- State indices -------------------------------------------------------
int ST_OPERATIONAL, ST_IDLE, ST_HAS_MONEY, ST_SELECTING, ST_CONFIRMED;
int ST_DISPENSING, ST_OUT_OF_SERVICE;

// ---- Events -------------------------------------------------------------
enum Events : uint8_t {
  EVT_COIN       = 1,   // payload: amount in cents
  EVT_SELECT,           // payload: item index (0-based)
  EVT_DISPENSE_DONE,
  EVT_FAULT,
  EVT_SERVICED,
};

// ---- Prices & credit ----------------------------------------------------
static const int PRICES[]  = { 150, 200, 100, 250 };  // cents
static const int N_ITEMS   = 4;
static int  credit         = 0;
static int  selectedItem   = -1;

// ---- Display helpers (implement for your hardware) ----------------------
void showIdle()        { Serial.print("Insert coins. Credit: "); Serial.println(credit); }
void showHasMoney()    { Serial.print("Select item. Credit: "); Serial.println(credit); }
void showDispensing()  { Serial.println("Dispensing…"); }
void showFault()       { Serial.println("OUT OF SERVICE"); }

// ---- OPERATIONAL superstate --------------------------------------------
bool operationalEvent(uint8_t e) {
  if (e == EVT_FAULT) {
    fsm.transitionTo(ST_OUT_OF_SERVICE);
    return true;
  }
  return false;  // other events bubble further (or are handled by children)
}

// ---- IDLE ---------------------------------------------------------------
void idleEntry() { credit = 0; showIdle(); }

bool idleEvent(uint8_t e) {
  if (e == EVT_COIN) {
    credit += (int)fsm.getEventData();
    fsm.transitionTo(ST_HAS_MONEY);   // → SELECTING (initial substate)
    return true;
  }
  return false;
}

// ---- HAS_MONEY superstate ----------------------------------------------
void hasMoney_entry() { showHasMoney(); }

bool hasMoney_event(uint8_t e) {
  if (e == EVT_COIN) {
    credit += (int)fsm.getEventData();
    showHasMoney();
    return true;
  }
  return false;
}

// ---- SELECTING ----------------------------------------------------------
bool selectingEvent(uint8_t e) {
  if (e == EVT_SELECT) {
    int item = (int)fsm.getEventData();
    if (item < 0 || item >= N_ITEMS) return true;   // ignore invalid

    if (credit >= PRICES[item]) {
      selectedItem = item;
      fsm.transitionTo(ST_CONFIRMED);
    } else {
      Serial.print("Need ");
      Serial.print(PRICES[item] - credit);
      Serial.println(" more cents.");
    }
    return true;
  }
  return false;
}

// ---- CONFIRMED ----------------------------------------------------------
void confirmedEntry() {
  // Immediately kick off the dispenser and move to DISPENSING
  fsm.transitionTo(ST_DISPENSING);
}

// ---- DISPENSING ---------------------------------------------------------
void dispensingEntry() {
  showDispensing();
  // Start the dispenser motor — in real code: digitalWrite(MOTOR_PIN, HIGH);
}
void dispensingExit() {
  // Stop the motor: digitalWrite(MOTOR_PIN, LOW);
}

bool dispensingEvent(uint8_t e) {
  if (e == EVT_DISPENSE_DONE) {
    credit -= PRICES[selectedItem];
    selectedItem = -1;
    if (credit >= 50)
      fsm.transitionTo(ST_HAS_MONEY);  // → SELECTING; keep remaining credit
    else
      fsm.transitionTo(ST_IDLE);       // → IDLE; credit returned to 0 in idleEntry
    return true;
  }
  return false;
}

// ---- OUT_OF_SERVICE -----------------------------------------------------
void oosEntry() { showFault(); }

bool oosEvent(uint8_t e) {
  if (e == EVT_SERVICED) {
    fsm.transitionTo(ST_OPERATIONAL);  // → IDLE (initial substate)
    return true;
  }
  return false;
}

// ---- setup / loop -------------------------------------------------------
void setup() {
  Serial.begin(115200);

  // Parents first
  ST_OPERATIONAL   = fsm.addState("OPERATIONAL", nullptr, nullptr,        nullptr,       0, -1, operationalEvent, -1);
  ST_OUT_OF_SERVICE= fsm.addState("OOS",         nullptr, oosEntry,       nullptr,       0, -1, oosEvent,         -1);
  ST_IDLE          = fsm.addState("IDLE",         nullptr, idleEntry,      nullptr,       0, -1, idleEvent,        ST_OPERATIONAL);
  ST_HAS_MONEY     = fsm.addState("HAS_MONEY",    nullptr, hasMoney_entry, nullptr,       0, -1, hasMoney_event,   ST_OPERATIONAL);
  ST_DISPENSING    = fsm.addState("DISPENSING",   nullptr, dispensingEntry,dispensingExit,0, -1, dispensingEvent,  ST_OPERATIONAL);
  ST_SELECTING     = fsm.addState("SELECTING",    nullptr, nullptr,        nullptr,       0, -1, selectingEvent,   ST_HAS_MONEY);
  ST_CONFIRMED     = fsm.addState("CONFIRMED",    nullptr, confirmedEntry, nullptr,       0, -1, nullptr,          ST_HAS_MONEY);

  // Initial substates
  fsm.setInitial(ST_OPERATIONAL, ST_IDLE);
  fsm.setInitial(ST_HAS_MONEY,   ST_SELECTING);

  fsm.begin(ST_OPERATIONAL);   // → IDLE
}

void loop() {
  // Simulate coin insertion: send 'c' for 100 cents
  if (Serial.available()) {
    char ch = Serial.read();
    if (ch == 'c') fsm.sendEvent(EVT_COIN, 100);
    if (ch >= '0' && ch <= '3') fsm.sendEvent(EVT_SELECT, ch - '0');
    if (ch == 'd') fsm.sendEvent(EVT_DISPENSE_DONE);
    if (ch == 'f') fsm.sendEvent(EVT_FAULT);
    if (ch == 's') fsm.sendEvent(EVT_SERVICED);
  }

  // Status display — using isInHierarchy() avoids coupling the display
  // code to the machine's internal state indices directly.
  static unsigned long lastDisplay = 0;
  if (millis() - lastDisplay > 2000) {
    lastDisplay = millis();
    Serial.print("State: ");
    Serial.print(fsm.getCurrentName());
    Serial.print("  Operational: ");
    Serial.println(fsm.isInHierarchy(ST_OPERATIONAL) ? "YES" : "NO");
  }

  fsm.update();
}
```

## Notes

- `CONFIRMED`'s `entry()` immediately calls `transitionTo(ST_DISPENSING)`. The
  transition is deferred until the current `update()` tick ends — `CONFIRMED`'s
  `entry()` runs, sets the pending state, and exits normally. One tick later the
  machine is in `DISPENSING`.
- `operationalEvent` returns `false` for any event it doesn't handle, so children
  can consume events like `EVT_COIN` and `EVT_SELECT` without them being swallowed
  by the superstate.
- Returning credit after dispensing is encapsulated entirely in `dispensingEvent` —
  no external code needs to know about it.
