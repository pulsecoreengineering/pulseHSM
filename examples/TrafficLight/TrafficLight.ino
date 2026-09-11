// TrafficLight — a flat timed state machine (the classic FSM).
//
// RED -> GREEN -> YELLOW -> RED on timers. Pressing the pedestrian button
// (send "1") while GREEN cuts the green short and goes to YELLOW early — an
// event pre-empting a timed transition.

#include "PulseHSM.h"

enum { ST_RED, ST_GREEN, ST_YELLOW };   // indices follow addState() order

PulseHSM fsm;
const uint8_t EVT_WALK = 1;

void redEntry()    { Serial.println(F("RED")); }
void greenEntry()  { Serial.println(F("GREEN")); }
void yellowEntry() { Serial.println(F("YELLOW")); }

// Only GREEN reacts to the walk button; other states ignore it (return false).
bool greenEvent(uint8_t e) {
    if (e == EVT_WALK) { Serial.println(F("  walk requested -> ending green")); fsm.transitionTo(ST_YELLOW); return true; }
    return false;
}

void setup() {
    Serial.begin(115200);
    //           name      update   entry        exit     timeoutMs next        onEvent      parent
    fsm.addState("red",    nullptr, redEntry,    nullptr, 5000,     ST_GREEN,  nullptr,     -1);
    fsm.addState("green",  nullptr, greenEntry,  nullptr, 5000,     ST_YELLOW, greenEvent,  -1);
    fsm.addState("yellow", nullptr, yellowEntry, nullptr, 2000,     ST_RED,    nullptr,     -1);
    fsm.begin(ST_RED);
}

void loop() {
    fsm.update();
    if (Serial.available() && Serial.read() == '1') fsm.sendEvent(EVT_WALK);
}
