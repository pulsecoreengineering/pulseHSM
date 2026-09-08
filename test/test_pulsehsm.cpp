// PulseHSM regression tests — pure host build, no board required.
//
//   Build & run BOTH modes (see test/run_tests.sh or the CI workflow):
//     g++ -std=c++11 -Wall -Wextra -I. -I.. test_pulsehsm.cpp ../PulseHSM.cpp -o t && ./t
//     g++ -std=c++11 -DPULSEHSM_SELF_TRANSITION_FULL_REINIT=1 ... && ./t
//
// Exit code 0 = all passed, non-zero = a regression (this is what CI checks).

#include <cstdio>
#include <cstring>
#include <cstdint>

unsigned long __pulsehsm_test_clock = 0;   // backing store for the shim clock

#include "PulseHSM.h"

// ---- tiny assert harness ----------------------------------------------------
static int g_failures = 0;
#define CHECK(cond) do { \
    if (!(cond)) { printf("  FAIL: %s  (line %d)\n", #cond, __LINE__); g_failures++; } \
} while (0)

// ---- shared trace of entry/exit/update calls --------------------------------
static char trace[512];
static void clearTrace() { trace[0] = '\0'; }
static void rec(const char* s) { strncat(trace, s, sizeof(trace) - strlen(trace) - 1); }

// Fixture: parent P with two leaf children A and B.
static PulseHSM fsm;
static int P, A, B;
static void pEntry() { rec("P+"); }  static void pExit() { rec("P-"); }
static void aEntry() { rec("A+"); }  static void aExit() { rec("A-"); }
static void bEntry() { rec("B+"); }  static void bExit() { rec("B-"); }
static void bUpdate() { rec("Bu"); }
static bool pEvent(uint8_t e) { if (e == 9) { rec("P!"); return true; } return false; }

static void buildFixture() {
    fsm = PulseHSM();
    P = fsm.addState("P", nullptr, pEntry, pExit, 0, -1, pEvent, -1);
    A = fsm.addState("A", nullptr, aEntry, aExit, 0, -1, nullptr, P);
    B = fsm.addState("B", bUpdate, bEntry, bExit, 0, -1, nullptr, P);
}

// ---- tests ------------------------------------------------------------------
static void test_begin_entry_order() {
    buildFixture(); clearTrace();
    CHECK(fsm.begin(A) == true);
    fsm.update();
    // outermost entry first: parent then child
    CHECK(strstr(trace, "P+A+") != nullptr);
    CHECK(fsm.getCurrentState() == A);
}

static void test_sibling_transition_keeps_parent() {
    buildFixture(); fsm.begin(A); fsm.update(); clearTrace();
    fsm.transitionTo(B); fsm.update();
    // exit A, enter B; shared parent P is NOT exited or re-entered
    CHECK(strcmp(trace, "A-B+") == 0);
}

static void test_event_bubbles_to_parent() {
    buildFixture(); fsm.begin(B); fsm.update(); clearTrace();
    fsm.sendEvent(9); fsm.update();
    CHECK(strstr(trace, "P!") != nullptr);   // parent handled the child's unhandled event
}

static void test_self_transition_mode() {
    // Note: B has an update() callback ("Bu"), which fires every tick regardless
    // of the transition, so we assert on the entry/exit markers specifically.
    buildFixture(); fsm.begin(B); fsm.update(); clearTrace();
    fsm.transitionTo(B); fsm.update();
#if PULSEHSM_SELF_TRANSITION_FULL_REINIT
    // full reinit: exit then re-enter THIS state only — ancestors untouched
    CHECK(strstr(trace, "B-B+") != nullptr);
    CHECK(strstr(trace, "P+") == nullptr && strstr(trace, "P-") == nullptr);
#else
    // lightweight: no entry/exit fired at all (only the update tick "Bu")
    CHECK(strstr(trace, "B+") == nullptr && strstr(trace, "B-") == nullptr);
    CHECK(strstr(trace, "P+") == nullptr && strstr(trace, "P-") == nullptr);
#endif
}

static void test_self_transition_resets_timer() {
    buildFixture(); fsm.begin(B); fsm.update();
    __pulsehsm_test_clock = 1000;
    CHECK(fsm.getStateElapsed() >= 1000);
    fsm.transitionTo(B); fsm.update();
    CHECK(fsm.getStateElapsed() == 0);       // timer reset in both modes
}

static void test_depth_guard() {
    // A chain deeper than PULSEHSM_MAX_DEPTH ancestors must be refused, not truncated.
    PulseHSM h;
    int prev = h.addState("root", nullptr, nullptr, nullptr, 0, -1, nullptr, -1);
    int lastGood = prev;
    for (int i = 0; i < PULSEHSM_MAX_DEPTH; i++) {
        int idx = h.addState("n", nullptr, nullptr, nullptr, 0, -1, nullptr, prev);
        CHECK(idx != -1);                    // up to MAX_DEPTH ancestors is allowed
        lastGood = idx;
        prev = idx;
    }
    int tooDeep = h.addState("toodeep", nullptr, nullptr, nullptr, 0, -1, nullptr, lastGood);
    CHECK(tooDeep == -1);                    // one deeper is rejected
}

static void test_leaf_precondition() {
    buildFixture();
    CHECK(fsm.begin(P) == false);            // P is composite -> refused
    CHECK(fsm.begin(A) == true);             // A is a leaf -> accepted
}

// File-scope capture so the plain-function callbacks below can reach it.
static PulseHSM payloadFsm;
static int32_t g_lastPayload = -1;
static bool payloadCb(uint8_t) { g_lastPayload = payloadFsm.getEventData(); return true; }

static void test_event_payload() {
    payloadFsm = PulseHSM();
    int s = payloadFsm.addState("s", nullptr, nullptr, nullptr, 0, -1, payloadCb, -1);
    payloadFsm.begin(s);
    payloadFsm.sendEvent(1, 424242);
    payloadFsm.update();
    CHECK(g_lastPayload == 424242);          // payload delivered to the handler
}

static char g_order[64];
static PulseHSM orderFsm;
static bool orderCb(uint8_t e) {
    char b[3] = { (char)('0' + e), ' ', '\0' };
    strncat(g_order, b, sizeof(g_order) - strlen(g_order) - 1);
    return true;
}

static void test_queue_fifo_order() {
    // The ring buffer must preserve FIFO order for a power-of-two size.
    g_order[0] = '\0';
    orderFsm = PulseHSM();
    int s = orderFsm.addState("s", nullptr, nullptr, nullptr, 0, -1, orderCb, -1);
    orderFsm.begin(s);
    for (uint8_t e = 1; e <= 6; e++) orderFsm.sendEvent(e);
    orderFsm.update();
    CHECK(strcmp(g_order, "1 2 3 4 5 6 ") == 0);
}

int main() {
    printf("PulseHSM tests (self-transition mode = %d)\n",
           PULSEHSM_SELF_TRANSITION_FULL_REINIT);
    test_begin_entry_order();
    test_sibling_transition_keeps_parent();
    test_event_bubbles_to_parent();
    test_self_transition_mode();
    test_self_transition_resets_timer();
    test_depth_guard();
    test_leaf_precondition();
    test_event_payload();
    test_queue_fifo_order();

    if (g_failures == 0) { printf("ALL PASSED\n"); return 0; }
    printf("%d CHECK(s) FAILED\n", g_failures);
    return 1;
}
