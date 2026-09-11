// ConnectionRetry — demonstrates self-transitions and PULSEHSM_SELF_TRANSITION_FULL_REINIT.
//
// CONNECTING retries by transitioning to itself. What a self-transition does
// depends on the mode you compile with:
//
//   Mode 0 (default): the self-transition only resets the state timer. entry()
//                     does NOT run, so you must reset per-attempt state yourself.
//   Mode 1:           the self-transition runs exit() then entry() for THIS
//                     state (ancestors untouched), so entry() re-arms each attempt.
//
// Flip the #define below and compare. This replaces the earlier broken demo.

#define PULSEHSM_SELF_TRANSITION_FULL_REINIT 0   // try 1 to see entry() re-run
#include "PulseHSM.h"

enum { ST_DISCONNECTED, ST_CONNECTING, ST_CONNECTED, ST_FAILED };

PulseHSM fsm;
const uint8_t EVT_CONNECT = 1;
const int MAX_RETRIES = 3;

int  retries = 0;
bool attemptStarted = false;

// Pretend the 2nd attempt succeeds. Replace with your real transport.
bool tryConnect() { return retries >= 2; }

void connectingEntry() {
    attemptStarted = false;                 // re-armed each entry (matters in Mode 1)
    Serial.print(F("CONNECTING (attempt ")); Serial.print(retries + 1); Serial.println(F(")"));
}
void connectingUpdate() {
    if (!attemptStarted) {
        attemptStarted = true;
        if (tryConnect()) { fsm.transitionTo(ST_CONNECTED); return; }
    }
    if (fsm.getStateElapsed() >= 1000) {    // this attempt's window elapsed
        retries++;
        if (retries >= MAX_RETRIES) { fsm.transitionTo(ST_FAILED); return; }
#if !PULSEHSM_SELF_TRANSITION_FULL_REINIT
        connectingEntry();                  // Mode 0: reset per-attempt state manually
#endif
        fsm.transitionTo(ST_CONNECTING);    // self-transition -> next attempt
    }
}
void connectedEntry() { Serial.println(F("CONNECTED")); }
void failedEntry()    { Serial.println(F("FAILED")); }

bool disconnectedEvent(uint8_t e) { if (e == EVT_CONNECT) { retries = 0; fsm.transitionTo(ST_CONNECTING); return true; } return false; }

void setup() {
    Serial.begin(115200);
    fsm.addState("disconnected", nullptr,           nullptr,        nullptr, 0, -1, disconnectedEvent, -1);
    fsm.addState("connecting",   connectingUpdate,  connectingEntry,nullptr, 0, -1, nullptr,           -1);
    fsm.addState("connected",    nullptr,           connectedEntry, nullptr, 0, -1, nullptr,           -1);
    fsm.addState("failed",       nullptr,           failedEntry,    nullptr, 0, -1, nullptr,           -1);
    fsm.begin(ST_DISCONNECTED);
    Serial.println(F("Send '1' to start connecting."));
}

void loop() {
    fsm.update();
    if (Serial.available() && Serial.read() == '1') fsm.sendEvent(EVT_CONNECT);
}
