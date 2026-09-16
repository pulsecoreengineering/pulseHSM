// ConnectionRetry — demonstrates self-transitions and PULSEHSM_SELF_TRANSITION_FULL_REINIT.
//
// CONNECTING retries by transitioning to itself. What a self-transition does
// depends on the compile-time flag:
//
//   Mode 0 (default): self-transition resets the state timer only. entry() does
//                     NOT re-run; per-attempt state must be reset manually.
//   Mode 1:           self-transition runs exit() then entry() for this state
//                     (ancestors untouched), so entry() re-arms each attempt.
//
// Flip the #define below and compare.

#define PULSEHSM_SELF_TRANSITION_FULL_REINIT 0   // try 1 to see entry() re-run
#include "PulseHSM.h"

enum StateID : int8_t { ST_DISCONNECTED = 0, ST_CONNECTING, ST_CONNECTED, ST_FAILED, ST_COUNT };
enum Evt     : uint8_t { EVT_CONNECT = 1 };

const int MAX_RETRIES = 3;

int  retries = 0;
bool attemptStarted = false;

bool tryConnect() { return retries >= 2; }   // pretend the 2nd attempt succeeds

// Forward declarations
bool  disconnectedEvent(uint8_t e);
void  connectingEntry();
void  connectingUpdate();
void  connectedEntry();
void  failedEntry();

//                                name               update          entry           exit     ms   next   event              parent  initialChild
constexpr PulseHSM::StaticState TABLE[ST_COUNT] PULSEHSM_TABLE = {
    [ST_DISCONNECTED] = { PULSEHSM_NAME("disconnected"), nullptr,        nullptr,        nullptr, 0,   -1,    disconnectedEvent, -1, -1 },
    [ST_CONNECTING]   = { PULSEHSM_NAME("connecting"),   connectingUpdate, connectingEntry, nullptr, 0,   -1,    nullptr,          -1, -1 },
    [ST_CONNECTED]    = { PULSEHSM_NAME("connected"),    nullptr,        connectedEntry, nullptr, 0,   -1,    nullptr,          -1, -1 },
    [ST_FAILED]       = { PULSEHSM_NAME("failed"),       nullptr,        failedEntry,    nullptr, 0,   -1,    nullptr,          -1, -1 },
};
PULSEHSM_VALIDATE_TABLE(TABLE, ST_COUNT);

PulseHSM fsm(TABLE, ST_COUNT);

bool disconnectedEvent(uint8_t e) {
    if (e == EVT_CONNECT) { retries = 0; fsm.transitionTo(ST_CONNECTING); return true; }
    return false;
}

void connectingEntry() {
    attemptStarted = false;
    Serial.print(F("CONNECTING (attempt ")); Serial.print(retries + 1); Serial.println(F(")"));
}

void connectingUpdate() {
    if (!attemptStarted) {
        attemptStarted = true;
        if (tryConnect()) { fsm.transitionTo(ST_CONNECTED); return; }
    }
    if (fsm.getStateElapsed() >= 1000) {
        retries++;
        if (retries >= MAX_RETRIES) { fsm.transitionTo(ST_FAILED); return; }
#if !PULSEHSM_SELF_TRANSITION_FULL_REINIT
        connectingEntry();    // Mode 0: reset per-attempt state manually
#endif
        fsm.transitionTo(ST_CONNECTING);   // self-transition → next attempt
    }
}

void connectedEntry() { Serial.println(F("CONNECTED")); }
void failedEntry()    { Serial.println(F("FAILED")); }

void setup() {
    Serial.begin(115200);
    fsm.begin(ST_DISCONNECTED);
    Serial.println(F("Send '1' to start connecting."));
}

void loop() {
    fsm.update();
    if (Serial.available() && Serial.read() == '1') fsm.sendEvent(EVT_CONNECT);
}
