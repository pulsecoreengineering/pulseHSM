// Integration test for AppStateMachine.h — runs the 7 behavioral checks from
// test_main.cpp adapted for the host test harness (uses __pulsehsm_test_clock).
//
// Build: g++ -std=gnu++11 -I. -I.. test_appstate.cpp ../PulseHSM.cpp -o t && ./t

#include <cstdio>
#include <cstring>
#include <string>

unsigned long __pulsehsm_test_clock = 0;

#include "AppStateMachine.h"

// Global FSM instance — table and count come from AppStateMachine.h.
PulseHSM fsm(HSM_STATE_TABLE, STATE_COUNT);

// Implement the callbacks forward-declared in AppStateMachine.h.
static std::string trace;
static void mark(const char* s) { trace += s; trace += " "; }

bool onEvent_System(uint8_t evt) {
    if (evt == EVT_ESTOP) { fsm.transitionTo(ST_STANDBY); return true; }
    return false;
}
void onEntry_Standby() { mark("STANDBY"); }
bool onEvent_Standby(uint8_t evt) {
    if (evt == EVT_START) { fsm.transitionTo(ST_RUNNING); return true; }
    return false;
}
void onEntry_WarmingUp() { mark("WARMING_UP"); }
void onEntry_Operating() { mark("OPERATING"); }
bool onEvent_Operating(uint8_t evt) {
    if (evt == EVT_STOP) { fsm.transitionTo(ST_STANDBY); return true; }
    return false;
}

static int failures = 0;
static void check(const char* what, const std::string& got, const char* want) {
    bool ok = (got == want);
    if (!ok) failures++;
    printf("  [%s] %-36s got: %s\n", ok ? "PASS" : "FAIL", what, got.c_str());
    if (!ok) printf("         %-36s want: %s\n", "", want);
}
static void advance(unsigned long ms) { __pulsehsm_test_clock += ms; }

int main() {
    printf("== AppStateMachine integration ==\n");

    // 1. Full nominal cycle
    trace.clear();
    fsm.begin(ST_STANDBY);
    fsm.sendEvent(EVT_START);
    fsm.update();
    advance(2999); fsm.update();
    advance(1);    fsm.update();
    fsm.sendEvent(EVT_STOP);
    fsm.update();
    check("nominal cycle", trace, "STANDBY WARMING_UP OPERATING STANDBY ");

    // 2. E-STOP bubbles from OPERATING up to SYSTEM
    trace.clear();
    __pulsehsm_test_clock = 0;
    fsm.begin(ST_STANDBY);
    fsm.sendEvent(EVT_START); fsm.update();
    advance(3000); fsm.update();
    fsm.sendEvent(EVT_ESTOP); fsm.update();
    check("estop bubbles to SYSTEM", trace, "STANDBY WARMING_UP OPERATING STANDBY ");

    // 3. Two events queued before a single update()
    trace.clear();
    __pulsehsm_test_clock = 0;
    fsm.begin(ST_STANDBY);
    fsm.sendEvent(EVT_START);
    fsm.sendEvent(EVT_ESTOP);
    fsm.update();
    check("queued START+ESTOP", trace, "STANDBY WARMING_UP STANDBY ");

    // 4. Timeout must fire exactly once
    trace.clear();
    __pulsehsm_test_clock = 0;
    fsm.begin(ST_STANDBY);
    fsm.sendEvent(EVT_START); fsm.update();
    advance(9000);
    for (int i = 0; i < 5; i++) fsm.update();
    check("timeout fires once", trace, "STANDBY WARMING_UP OPERATING ");

    // 5. Unhandled event is harmless
    trace.clear();
    __pulsehsm_test_clock = 0;
    fsm.begin(ST_STANDBY);
    fsm.sendEvent(EVT_STOP);
    fsm.update();
    check("unhandled event ignored", trace, "STANDBY ");

    // 6. isInHierarchy() reports ancestors correctly
    trace.clear();
    __pulsehsm_test_clock = 0;
    fsm.begin(ST_STANDBY);
    fsm.sendEvent(EVT_START); fsm.update();
    {
        std::string s;
        s += fsm.isInHierarchy(ST_RUNNING) ? "in-RUNNING "     : "NOT-in-RUNNING ";
        s += fsm.isInHierarchy(ST_SYSTEM)  ? "in-SYSTEM "      : "NOT-in-SYSTEM ";
        s += fsm.isInHierarchy(ST_STANDBY) ? "in-STANDBY "     : "NOT-in-STANDBY ";
        s += fsm.getCurrentName();
        check("hierarchy query", s, "in-RUNNING in-SYSTEM NOT-in-STANDBY WARMING_UP");
    }

    // 7. Event payload survives to the handler
    trace.clear();
    __pulsehsm_test_clock = 0;
    fsm.begin(ST_STANDBY);
    fsm.sendEvent(EVT_START, 42); fsm.update();
    check("payload delivered",
          std::string(fsm.getEventData() == 42 ? "42" : "lost"), "42");

    printf("  %s\n\n", failures ? "*** FAILURES ***" : "all checks passed");
    return failures ? 1 : 0;
}
