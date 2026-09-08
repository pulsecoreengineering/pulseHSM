// BlinkHSM — a minimal, correct PulseHSM example.
//
// Two leaf states, ON and OFF, auto-toggle every 500 ms. Send event 1 (type "1"
// + Enter in the Serial Monitor) to latch the LED off; send 1 again to resume.
// Demonstrates leaf states, timed transitions, and event handlers with the
// correct `bool` return type.

#include "PulseHSM.h"

// State indices are assigned by addState() in call order starting at 0, so we
// can name them up front and use them as forward references for timeoutNext.
enum { ST_ON, ST_OFF, ST_LATCHED };

PulseHSM fsm;

const int LED = LED_BUILTIN;
const uint8_t EVT_TOGGLE = 1;

void onEntry()      { digitalWrite(LED, HIGH); }
void offEntry()     { digitalWrite(LED, LOW); }
void latchedEntry() { digitalWrite(LED, LOW); Serial.println(F("LATCHED — send 1 to release")); }

// Handlers MUST return bool: true = handled, false = bubble up.
bool onEvent(uint8_t e)      { if (e == EVT_TOGGLE) { fsm.transitionTo(ST_LATCHED); return true; } return false; }
bool offEvent(uint8_t e)     { if (e == EVT_TOGGLE) { fsm.transitionTo(ST_LATCHED); return true; } return false; }
bool latchedEvent(uint8_t e) { if (e == EVT_TOGGLE) { fsm.transitionTo(ST_OFF);     return true; } return false; }

void setup() {
    Serial.begin(115200);
    pinMode(LED, OUTPUT);

    //           name       update   entry         exit     timeoutMs  next      onEvent       parent
    fsm.addState("on",      nullptr, onEntry,      nullptr, 500,       ST_OFF,   onEvent,      -1);  // -> ST_ON
    fsm.addState("off",     nullptr, offEntry,     nullptr, 500,       ST_ON,    offEvent,     -1);  // -> ST_OFF
    fsm.addState("latched", nullptr, latchedEntry, nullptr, 0,         -1,       latchedEvent, -1);  // -> ST_LATCHED

    if (!fsm.begin(ST_ON)) {
        Serial.println(F("begin() failed — start state must be a leaf."));
    }
}

void loop() {
    fsm.update();
    if (Serial.available() && Serial.read() == '1') fsm.sendEvent(EVT_TOGGLE);
}
