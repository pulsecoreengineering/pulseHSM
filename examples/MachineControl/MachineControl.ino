// MachineControl — the reason to use a *hierarchical* state machine.
//
// A RUNNING superstate contains three substates (STARTING, RUNNING_NORMAL,
// STOPPING). The emergency stop is handled ONCE, on the RUNNING parent: an
// EVT_ESTOP raised in any substate bubbles up and trips the machine to FAULT.
// You don't repeat the e-stop handler in every child.
//
// Entering any substate runs RUNNING's entry() once ("motor power ON"); leaving
// RUNNING for FAULT runs its exit() once ("motor power OFF"). That's the
// entry/exit chaining doing the safety wiring for you.
//
//   Send:  s = start   x = e-stop   t = stop   r = reset
//
// State tree:
//   IDLE
//   FAULT
//   RUNNING            (superstate: powers the motor, owns e-stop)
//     STARTING         (1s) -> RUNNING_NORMAL
//     RUNNING_NORMAL   -- EVT_STOP --> STOPPING
//     STOPPING         (1s) -> IDLE

#include "PulseHSM.h"

// addState() order == index order; parents must come before their children.
enum { ST_IDLE, ST_FAULT, ST_RUNNING, ST_STARTING, ST_RUN_NORMAL, ST_STOPPING };

PulseHSM fsm;
const uint8_t EVT_START = 's', EVT_ESTOP = 'x', EVT_STOP = 't', EVT_RESET = 'r';

void idleEntry()      { Serial.println(F("IDLE  (send 's' to start)")); }
void faultEntry()     { Serial.println(F("FAULT (send 'r' to reset)")); }
void runningEntry()   { Serial.println(F("  [motor power ON]")); }
void runningExit()    { Serial.println(F("  [motor power OFF]")); }
void startingEntry()  { Serial.println(F("STARTING...")); }
void runNormalEntry() { Serial.println(F("RUNNING")); }
void stoppingEntry()  { Serial.println(F("STOPPING...")); }

bool idleEvent(uint8_t e)     { if (e == EVT_START) { fsm.transitionTo(ST_STARTING); return true; } return false; }
bool runNormalEvent(uint8_t e){ if (e == EVT_STOP)  { fsm.transitionTo(ST_STOPPING); return true; } return false; }
bool faultEvent(uint8_t e)    { if (e == EVT_RESET) { fsm.transitionTo(ST_IDLE);     return true; } return false; }

// Handled on the PARENT: applies to every RUNNING substate via event bubbling.
bool runningEvent(uint8_t e)  { if (e == EVT_ESTOP) { Serial.println(F("  !! E-STOP !!")); fsm.transitionTo(ST_FAULT); return true; } return false; }

void setup() {
    Serial.begin(115200);
    //           name             update   entry           exit          timeoutMs next            onEvent         parent
    fsm.addState("idle",          nullptr, idleEntry,      nullptr,      0,        -1,            idleEvent,      -1);
    fsm.addState("fault",         nullptr, faultEntry,     nullptr,      0,        -1,            faultEvent,     -1);
    fsm.addState("running",       nullptr, runningEntry,   runningExit,  0,        -1,            runningEvent,   -1);
    fsm.addState("starting",      nullptr, startingEntry,  nullptr,      1000,     ST_RUN_NORMAL, nullptr,        ST_RUNNING);
    fsm.addState("running_norm",  nullptr, runNormalEntry, nullptr,      0,        -1,            runNormalEvent, ST_RUNNING);
    fsm.addState("stopping",      nullptr, stoppingEntry,  nullptr,      1000,     ST_IDLE,       nullptr,        ST_RUNNING);
    fsm.begin(ST_IDLE);
}

void loop() {
    fsm.update();
    if (Serial.available()) {
        int c = Serial.read();
        if (c == 's' || c == 'x' || c == 't' || c == 'r') fsm.sendEvent((uint8_t)c);
    }
}
