// ButtonDebounce — debounce a physical button as a state machine.
//
// The raw pin is sampled in each state's update(). A change starts a short
// debounce window (a timed state); the press/release is only committed if the
// level is still there when the window expires. Demonstrates update() polling
// plus getStateElapsed()-style timed settling without extra timers.

#include "PulseHSM.h"

enum { ST_RELEASED, ST_SETTLING_PRESS, ST_PRESSED, ST_SETTLING_RELEASE };

PulseHSM fsm;
const int BUTTON = 2;            // active-low button to GND, INPUT_PULLUP
const unsigned long DEBOUNCE_MS = 25;

inline bool rawPressed() { return digitalRead(BUTTON) == LOW; }

void releasedUpdate()      { if (rawPressed())  fsm.transitionTo(ST_SETTLING_PRESS); }
void settlingPressUpdate() { if (!rawPressed()) fsm.transitionTo(ST_RELEASED); }   // bounced back
void pressedEntry()        { Serial.println(F("CLICK")); }
void pressedUpdate()       { if (!rawPressed()) fsm.transitionTo(ST_SETTLING_RELEASE); }
void settlingReleaseUpdate(){ if (rawPressed()) fsm.transitionTo(ST_PRESSED); }    // bounced back

void setup() {
    Serial.begin(115200);
    pinMode(BUTTON, INPUT_PULLUP);
    //           name              update                 entry         exit     timeoutMs    next              onEvent  parent
    fsm.addState("released",       releasedUpdate,        nullptr,      nullptr, 0,           -1,              nullptr, -1);
    fsm.addState("settling_press", settlingPressUpdate,   nullptr,      nullptr, DEBOUNCE_MS, ST_PRESSED,      nullptr, -1);
    fsm.addState("pressed",        pressedUpdate,         pressedEntry, nullptr, 0,           -1,              nullptr, -1);
    fsm.addState("settling_rel",   settlingReleaseUpdate, nullptr,      nullptr, DEBOUNCE_MS, ST_RELEASED,     nullptr, -1);
    fsm.begin(ST_RELEASED);
}

void loop() {
    fsm.update();
}
