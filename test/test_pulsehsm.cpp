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

// ---- Initial substate tests -------------------------------------------------
// Fixture: IP -> { IA -> { IC }, IB }
//   IP.initialChild = IA,  IA.initialChild = IC
static PulseHSM initFsm;
static int IP, IA, IB, IC;
static char initTrace[256];
static void clearInitTrace() { initTrace[0] = '\0'; }
static void ipEntry() { strncat(initTrace, "P+", sizeof(initTrace) - strlen(initTrace) - 1); }
static void ipExit()  { strncat(initTrace, "P-", sizeof(initTrace) - strlen(initTrace) - 1); }
static void iaEntry() { strncat(initTrace, "A+", sizeof(initTrace) - strlen(initTrace) - 1); }
static void iaExit()  { strncat(initTrace, "A-", sizeof(initTrace) - strlen(initTrace) - 1); }
static void ibEntry() { strncat(initTrace, "B+", sizeof(initTrace) - strlen(initTrace) - 1); }
static void ibExit()  { strncat(initTrace, "B-", sizeof(initTrace) - strlen(initTrace) - 1); }
static void icEntry() { strncat(initTrace, "C+", sizeof(initTrace) - strlen(initTrace) - 1); }
static void icExit()  { strncat(initTrace, "C-", sizeof(initTrace) - strlen(initTrace) - 1); }

static void buildInitFixture() {
    initFsm = PulseHSM();
    IP = initFsm.addState("P", nullptr, ipEntry, ipExit, 0, -1, nullptr, -1);
    IA = initFsm.addState("A", nullptr, iaEntry, iaExit, 0, -1, nullptr, IP);
    IB = initFsm.addState("B", nullptr, ibEntry, ibExit, 0, -1, nullptr, IP);
    IC = initFsm.addState("C", nullptr, icEntry, icExit, 0, -1, nullptr, IA);
    CHECK(initFsm.setInitial(IP, IA) == true);
    CHECK(initFsm.setInitial(IA, IC) == true);
}

static void test_initial_substate_begin() {
    buildInitFixture(); clearInitTrace();
    CHECK(initFsm.begin(IP) == true);         // composite with initial child is valid
    CHECK(initFsm.getCurrentState() == IC);   // resolved to deepest leaf
    CHECK(strcmp(initTrace, "P+A+C+") == 0);  // outer-to-inner entry order
}

static void test_initial_substate_transition() {
    buildInitFixture();
    initFsm.begin(IB); initFsm.update(); clearInitTrace();
    initFsm.transitionTo(IP); initFsm.update();
    // IB exits, IP is LCA (not exited), IA and IC enter in order
    CHECK(strcmp(initTrace, "B-A+C+") == 0);
    CHECK(initFsm.getCurrentState() == IC);
}

static void test_initial_substate_nested() {
    // Three-level initial chain: R -> M (initial) -> L (initial)
    PulseHSM h;
    static char tr[64]; tr[0] = '\0';
    int R = h.addState("R", nullptr, [](){strncat(tr,"R+",sizeof(tr)-strlen(tr)-1);},
                       nullptr, 0, -1, nullptr, -1);
    int M = h.addState("M", nullptr, [](){strncat(tr,"M+",sizeof(tr)-strlen(tr)-1);},
                       nullptr, 0, -1, nullptr, R);
    int L = h.addState("L", nullptr, [](){strncat(tr,"L+",sizeof(tr)-strlen(tr)-1);},
                       nullptr, 0, -1, nullptr, M);
    int S = h.addState("S", nullptr, nullptr, nullptr, 0, -1, nullptr, -1);
    h.setInitial(R, M);
    h.setInitial(M, L);
    h.begin(S); h.update(); tr[0] = '\0';
    h.transitionTo(R); h.update();
    CHECK(h.getCurrentState() == L);
    CHECK(strcmp(tr, "R+M+L+") == 0);  // all three entered outer-to-inner
}

static void test_initial_substate_no_default() {
    PulseHSM h;
    int p = h.addState("P", nullptr, nullptr, nullptr, 0, -1, nullptr, -1);
    int a = h.addState("A", nullptr, nullptr, nullptr, 0, -1, nullptr, p);
    (void)a;
    // P is composite but has no initial child -> begin(P) must fail
    CHECK(h.begin(p) == false);
}

static void test_setInitial_validates_args() {
    buildInitFixture();
    CHECK(initFsm.setInitial(-1, IB) == false);  // bad parent index
    CHECK(initFsm.setInitial(IP, -1) == false);  // bad child index
    CHECK(initFsm.setInitial(IP, IC) == false);  // IC is grandchild, not direct child
}

static void test_leaf_precondition_still_works() {
    // Without setInitial, composite is still rejected by begin().
    buildFixture();
    CHECK(fsm.begin(P) == false);  // composite, no initialChild set
    CHECK(fsm.begin(A) == true);   // leaf accepted
}

// ---- sendEvent overflow returns false ---------------------------------------
static void test_queue_overflow_returns_false() {
    PulseHSM h;
    int s = h.addState("s", nullptr, nullptr, nullptr, 0, -1, nullptr, -1);
    h.begin(s);
    bool allQueued = true;
    for (int i = 0; i < PULSEHSM_MAX_EVENTS; i++)
        if (!h.sendEvent(1)) allQueued = false;
    CHECK(allQueued);          // first PULSEHSM_MAX_EVENTS events fit
    CHECK(!h.sendEvent(1));    // next one overflows -> false
    h.update();                // drain — no crash
}

// ---- Multiple independent instances don't interfere -------------------------
static void test_multi_instance_isolation() {
    PulseHSM mA, mB;
    int sA1 = mA.addState("A1", nullptr, nullptr, nullptr, 0, -1, nullptr, -1);
    int sA2 = mA.addState("A2", nullptr, nullptr, nullptr, 0, -1, nullptr, -1);
    int sB1 = mB.addState("B1", nullptr, nullptr, nullptr, 0, -1, nullptr, -1);
    int sB2 = mB.addState("B2", nullptr, nullptr, nullptr, 0, -1, nullptr, -1);
    mA.begin(sA1); mA.update();
    mB.begin(sB1); mB.update();
    mA.transitionTo(sA2); mA.update();
    CHECK(mA.getCurrentState() == sA2);
    CHECK(mB.getCurrentState() == sB1);  // B untouched
    mB.transitionTo(sB2); mB.update();
    CHECK(mA.getCurrentState() == sA2);  // A untouched
    CHECK(mB.getCurrentState() == sB2);
}

// ---- isInHierarchy across multiple levels -----------------------------------
static void test_is_in_hierarchy_levels() {
    buildFixture(); fsm.begin(A); fsm.update();
    CHECK(fsm.isInHierarchy(A) == true);   // current leaf
    CHECK(fsm.isInHierarchy(P) == true);   // active ancestor
    CHECK(fsm.isInHierarchy(B) == false);  // sibling, not active
}

// ---- Reentrancy: transitionTo() called from inside entry() ------------------
static PulseHSM reentrantFsm;
static int RS0, RS1, RS2;
static bool g_reentrantFired = false;
static void rs1Entry() {
    if (!g_reentrantFired) {
        g_reentrantFired = true;
        reentrantFsm.transitionTo(RS2);  // deferred — inTransition guard fires
    }
}

static void test_reentrant_transition() {
    reentrantFsm = PulseHSM();
    RS0 = reentrantFsm.addState("S0", nullptr, nullptr,  nullptr, 0, -1, nullptr, -1);
    RS1 = reentrantFsm.addState("S1", nullptr, rs1Entry, nullptr, 0, -1, nullptr, -1);
    RS2 = reentrantFsm.addState("S2", nullptr, nullptr,  nullptr, 0, -1, nullptr, -1);
    g_reentrantFired = false;
    reentrantFsm.begin(RS0); reentrantFsm.update();
    reentrantFsm.transitionTo(RS1); reentrantFsm.update();
    // rs1Entry ran and set pendingState=RS2; the inTransition flag deferred it.
    reentrantFsm.update();  // apply the deferred transition
    CHECK(reentrantFsm.getCurrentState() == RS2);
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
    // initial substate
    test_initial_substate_begin();
    test_initial_substate_transition();
    test_initial_substate_nested();
    test_initial_substate_no_default();
    test_setInitial_validates_args();
    test_leaf_precondition_still_works();
    // overflow / multi-instance / hierarchy / reentrancy
    test_queue_overflow_returns_false();
    test_multi_instance_isolation();
    test_is_in_hierarchy_levels();
    test_reentrant_transition();

    if (g_failures == 0) { printf("ALL PASSED\n"); return 0; }
    printf("%d CHECK(s) FAILED\n", g_failures);
    return 1;
}
