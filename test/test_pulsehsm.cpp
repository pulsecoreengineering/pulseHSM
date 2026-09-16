// PulseHSM v2 regression tests — pure host build, no board required.
//
//   Build & run BOTH modes (see test/run_tests.sh or the CI workflow):
//     g++ -std=gnu++11 -Wall -Wextra -I. -I.. test_pulsehsm.cpp ../PulseHSM.cpp -o t && ./t
//
// Exit code 0 = all passed, non-zero = a regression (this is what CI checks).

#include <cstdio>
#include <cstring>
#include <cstdint>

unsigned long __pulsehsm_test_clock = 0;

#include "PulseHSM.h"

// ── tiny assert harness ──────────────────────────────────────────────────────
static int g_failures = 0;
#define CHECK(cond) do { \
    if (!(cond)) { printf("  FAIL: %s  (line %d)\n", #cond, __LINE__); g_failures++; } \
} while(0)

// ── shared trace ──────────────────────────────────────────────────────────────
static char trace[512];
static void clearTrace() { trace[0] = '\0'; }
static void rec(const char* s) { strncat(trace, s, sizeof(trace) - strlen(trace) - 1); }

// =============================================================================
// FIXTURE 1: P (composite, initialChild=A) -> { A (leaf), B (leaf + update) }
// Pattern used throughout: forward-declare callbacks, define table, define FSM,
// then implement callbacks — so FSM address is valid when callback bodies run.
// =============================================================================
enum FX : int8_t { FX_P=0, FX_A, FX_B, FX_COUNT };

static void fxPEntry();  static void fxPExit();
static void fxAEntry();  static void fxAExit();
static void fxBEntry();  static void fxBExit();  static void fxBUpdate();
static bool fxPEvent(uint8_t e);

static constexpr PulseHSM::StaticState FX_TABLE[FX_COUNT] PULSEHSM_TABLE = {
    // FX_P: composite, initialChild=FX_A
    { PULSEHSM_NAME("P"), nullptr,   fxPEntry, fxPExit, 0, -1, fxPEvent, -1,   FX_A },
    // FX_A: leaf child of FX_P
    { PULSEHSM_NAME("A"), nullptr,   fxAEntry, fxAExit, 0, -1, nullptr,  FX_P, -1   },
    // FX_B: leaf child of FX_P, with update callback
    { PULSEHSM_NAME("B"), fxBUpdate, fxBEntry, fxBExit, 0, -1, nullptr,  FX_P, -1   },
};
PULSEHSM_VALIDATE_TABLE(FX_TABLE, FX_COUNT);
static PulseHSM fsm(FX_TABLE, FX_COUNT);

// Callback implementations — fsm is fully constructed above.
static void fxPEntry()  { rec("P+"); } static void fxPExit()  { rec("P-"); }
static void fxAEntry()  { rec("A+"); } static void fxAExit()  { rec("A-"); }
static void fxBEntry()  { rec("B+"); } static void fxBExit()  { rec("B-"); }
static void fxBUpdate() { rec("Bu"); }
static bool fxPEvent(uint8_t e) { if (e == 9) { rec("P!"); return true; } return false; }

static void resetFx(int startState) {
    __pulsehsm_test_clock = 0;
    clearTrace();
    fsm.begin(startState);
}

// ── Tests using FIXTURE 1 ─────────────────────────────────────────────────────

static void test_begin_entry_order() {
    resetFx(FX_A);
    fsm.update();
    CHECK(strstr(trace, "P+A+") != nullptr);
    CHECK(fsm.getCurrentState() == FX_A);
}

static void test_sibling_transition_keeps_parent() {
    resetFx(FX_A); fsm.update(); clearTrace();
    fsm.transitionTo(FX_B); fsm.update();
    CHECK(strcmp(trace, "A-B+") == 0);
}

static void test_event_bubbles_to_parent() {
    resetFx(FX_B); fsm.update(); clearTrace();
    fsm.sendEvent(9); fsm.update();
    CHECK(strstr(trace, "P!") != nullptr);
}

static void test_self_transition_mode() {
    resetFx(FX_B); fsm.update(); clearTrace();
    fsm.transitionTo(FX_B); fsm.update();
#if PULSEHSM_SELF_TRANSITION_FULL_REINIT
    CHECK(strstr(trace, "B-B+") != nullptr);
    CHECK(strstr(trace, "P+") == nullptr && strstr(trace, "P-") == nullptr);
#else
    CHECK(strstr(trace, "B+") == nullptr && strstr(trace, "B-") == nullptr);
    CHECK(strstr(trace, "P+") == nullptr && strstr(trace, "P-") == nullptr);
#endif
}

static void test_self_transition_resets_timer() {
    resetFx(FX_B); fsm.update();
    __pulsehsm_test_clock = 1000;
    CHECK(fsm.getStateElapsed() >= 1000);
    fsm.transitionTo(FX_B); fsm.update();
    CHECK(fsm.getStateElapsed() == 0);
}

static void test_begin_composite_resolves_to_leaf() {
    clearTrace();
    bool ok = fsm.begin(FX_P);
    CHECK(ok == true);
    CHECK(fsm.getCurrentState() == FX_A);
}

static void test_begin_out_of_range_fails() {
    CHECK(fsm.begin(-1)       == false);
    CHECK(fsm.begin(FX_COUNT) == false);
}

static void test_is_in_hierarchy_levels() {
    resetFx(FX_A); fsm.update();
    CHECK(fsm.isInHierarchy(FX_A) == true);
    CHECK(fsm.isInHierarchy(FX_P) == true);
    CHECK(fsm.isInHierarchy(FX_B) == false);
}

// =============================================================================
// FIXTURE 2: event payload
// =============================================================================
static int32_t g_lastPayload = -1;
static bool payloadCb(uint8_t e);   // forward-declare before table

enum PAY : int8_t { PAY_S=0, PAY_COUNT };
static constexpr PulseHSM::StaticState PAY_TABLE[PAY_COUNT] PULSEHSM_TABLE = {
    { PULSEHSM_NAME("s"), nullptr, nullptr, nullptr, 0, -1, payloadCb, -1, -1 },
};
PULSEHSM_VALIDATE_TABLE(PAY_TABLE, PAY_COUNT);
static PulseHSM payloadFsm(PAY_TABLE, PAY_COUNT);

static bool payloadCb(uint8_t) { g_lastPayload = payloadFsm.getEventData(); return true; }

static void test_event_payload() {
    g_lastPayload = -1;
    payloadFsm.begin(PAY_S);
    payloadFsm.sendEvent(1, 424242);
    payloadFsm.update();
    CHECK(g_lastPayload == 424242);
}

// =============================================================================
// FIXTURE 3: FIFO order (no FSM reference in callback)
// =============================================================================
static char g_order[64];
static bool orderCb(uint8_t e) {
    char b[3] = { (char)('0' + e), ' ', '\0' };
    strncat(g_order, b, sizeof(g_order) - strlen(g_order) - 1);
    return true;
}

enum ORD : int8_t { ORD_S=0, ORD_COUNT };
static constexpr PulseHSM::StaticState ORD_TABLE[ORD_COUNT] PULSEHSM_TABLE = {
    { PULSEHSM_NAME("s"), nullptr, nullptr, nullptr, 0, -1, orderCb, -1, -1 },
};
PULSEHSM_VALIDATE_TABLE(ORD_TABLE, ORD_COUNT);
static PulseHSM orderFsm(ORD_TABLE, ORD_COUNT);

static void test_queue_fifo_order() {
    g_order[0] = '\0';
    orderFsm.begin(ORD_S);
    for (uint8_t e = 1; e <= 6; e++) orderFsm.sendEvent(e);
    orderFsm.update();
    CHECK(strcmp(g_order, "1 2 3 4 5 6 ") == 0);
}

// =============================================================================
// FIXTURE 4: queue overflow + getDroppedEvents()
// =============================================================================
enum OVF : int8_t { OVF_S=0, OVF_COUNT };
static constexpr PulseHSM::StaticState OVF_TABLE[OVF_COUNT] PULSEHSM_TABLE = {
    { PULSEHSM_NAME("s"), nullptr, nullptr, nullptr, 0, -1, nullptr, -1, -1 },
};
PULSEHSM_VALIDATE_TABLE(OVF_TABLE, OVF_COUNT);

static void test_queue_overflow_returns_false() {
    PulseHSM h(OVF_TABLE, OVF_COUNT);
    h.begin(OVF_S);
    bool allQueued = true;
    for (int i = 0; i < PULSEHSM_MAX_EVENTS; i++)
        if (!h.sendEvent(1)) allQueued = false;
    CHECK(allQueued);
    CHECK(!h.sendEvent(1));
    CHECK(h.getDroppedEvents() == 1);
    h.update();
}

static void test_dropped_events_saturate() {
    PulseHSM h(OVF_TABLE, OVF_COUNT);
    h.begin(OVF_S);
    for (int i = 0; i < PULSEHSM_MAX_EVENTS; i++) h.sendEvent(1);
    for (int i = 0; i < 300; i++) h.sendEvent(1);
    CHECK(h.getDroppedEvents() == 255);   // saturates at 255
}

// =============================================================================
// FIXTURE 5: initial substate
// Tree: IP (initial=IA) -> { IA (initial=IC) -> { IC }, IB }
// =============================================================================
static char initTrace[256];
static void clearInitTrace() { initTrace[0] = '\0'; }
static void ipEntry() { strncat(initTrace, "P+", sizeof(initTrace)-strlen(initTrace)-1); }
static void ipExit()  { strncat(initTrace, "P-", sizeof(initTrace)-strlen(initTrace)-1); }
static void iaEntry() { strncat(initTrace, "A+", sizeof(initTrace)-strlen(initTrace)-1); }
static void iaExit()  { strncat(initTrace, "A-", sizeof(initTrace)-strlen(initTrace)-1); }
static void ibEntry() { strncat(initTrace, "B+", sizeof(initTrace)-strlen(initTrace)-1); }
static void ibExit()  { strncat(initTrace, "B-", sizeof(initTrace)-strlen(initTrace)-1); }
static void icEntry() { strncat(initTrace, "C+", sizeof(initTrace)-strlen(initTrace)-1); }
static void icExit()  { strncat(initTrace, "C-", sizeof(initTrace)-strlen(initTrace)-1); }

enum INIT : int8_t { INIT_P=0, INIT_A, INIT_B, INIT_C, INIT_COUNT };
static constexpr PulseHSM::StaticState INIT_TABLE[INIT_COUNT] PULSEHSM_TABLE = {
    // INIT_P: composite, parent=-1, initialChild=INIT_A
    { PULSEHSM_NAME("P"), nullptr, ipEntry, ipExit, 0, -1, nullptr, -1,     INIT_A },
    // INIT_A: composite, parent=INIT_P, initialChild=INIT_C
    { PULSEHSM_NAME("A"), nullptr, iaEntry, iaExit, 0, -1, nullptr, INIT_P, INIT_C },
    // INIT_B: leaf, parent=INIT_P
    { PULSEHSM_NAME("B"), nullptr, ibEntry, ibExit, 0, -1, nullptr, INIT_P, -1     },
    // INIT_C: leaf, parent=INIT_A
    { PULSEHSM_NAME("C"), nullptr, icEntry, icExit, 0, -1, nullptr, INIT_A, -1     },
};
PULSEHSM_VALIDATE_TABLE(INIT_TABLE, INIT_COUNT);
static PulseHSM initFsm(INIT_TABLE, INIT_COUNT);

static void test_initial_substate_begin() {
    clearInitTrace();
    CHECK(initFsm.begin(INIT_P) == true);
    CHECK(initFsm.getCurrentState() == INIT_C);
    CHECK(strcmp(initTrace, "P+A+C+") == 0);
}

static void test_initial_substate_transition() {
    clearInitTrace();
    initFsm.begin(INIT_B); initFsm.update(); clearInitTrace();
    initFsm.transitionTo(INIT_P); initFsm.update();
    CHECK(strcmp(initTrace, "B-A+C+") == 0);
    CHECK(initFsm.getCurrentState() == INIT_C);
}

// =============================================================================
// FIXTURE 6: nested 3-level initial chain  R -> M -> L, with sibling root S
// =============================================================================
static char nestTrace[64];
static void nrEntry() { strncat(nestTrace, "R+", sizeof(nestTrace)-strlen(nestTrace)-1); }
static void nmEntry() { strncat(nestTrace, "M+", sizeof(nestTrace)-strlen(nestTrace)-1); }
static void nlEntry() { strncat(nestTrace, "L+", sizeof(nestTrace)-strlen(nestTrace)-1); }

enum NEST : int8_t { NEST_R=0, NEST_M, NEST_L, NEST_S, NEST_COUNT };
static constexpr PulseHSM::StaticState NEST_TABLE[NEST_COUNT] PULSEHSM_TABLE = {
    { PULSEHSM_NAME("R"), nullptr, nrEntry, nullptr, 0, -1, nullptr, -1,     NEST_M },
    { PULSEHSM_NAME("M"), nullptr, nmEntry, nullptr, 0, -1, nullptr, NEST_R, NEST_L },
    { PULSEHSM_NAME("L"), nullptr, nlEntry, nullptr, 0, -1, nullptr, NEST_M, -1     },
    { PULSEHSM_NAME("S"), nullptr, nullptr, nullptr, 0, -1, nullptr, -1,     -1     },
};
PULSEHSM_VALIDATE_TABLE(NEST_TABLE, NEST_COUNT);
static PulseHSM nestFsm(NEST_TABLE, NEST_COUNT);

static void test_initial_substate_nested() {
    nestTrace[0] = '\0';
    nestFsm.begin(NEST_S); nestFsm.update(); nestTrace[0] = '\0';
    nestFsm.transitionTo(NEST_R); nestFsm.update();
    CHECK(nestFsm.getCurrentState() == NEST_L);
    CHECK(strcmp(nestTrace, "R+M+L+") == 0);
}

// =============================================================================
// FIXTURE 7: multiple independent instances
// =============================================================================
enum MA : int8_t { MA1=0, MA2, MA_COUNT };
enum MB : int8_t { MB1=0, MB2, MB_COUNT };

static constexpr PulseHSM::StaticState MA_TABLE[MA_COUNT] PULSEHSM_TABLE = {
    { PULSEHSM_NAME("A1"), nullptr, nullptr, nullptr, 0, -1, nullptr, -1, -1 },
    { PULSEHSM_NAME("A2"), nullptr, nullptr, nullptr, 0, -1, nullptr, -1, -1 },
};
PULSEHSM_VALIDATE_TABLE(MA_TABLE, MA_COUNT);

static constexpr PulseHSM::StaticState MB_TABLE[MB_COUNT] PULSEHSM_TABLE = {
    { PULSEHSM_NAME("B1"), nullptr, nullptr, nullptr, 0, -1, nullptr, -1, -1 },
    { PULSEHSM_NAME("B2"), nullptr, nullptr, nullptr, 0, -1, nullptr, -1, -1 },
};
PULSEHSM_VALIDATE_TABLE(MB_TABLE, MB_COUNT);

static void test_multi_instance_isolation() {
    PulseHSM mA(MA_TABLE, MA_COUNT), mB(MB_TABLE, MB_COUNT);
    mA.begin(MA1); mA.update();
    mB.begin(MB1); mB.update();
    mA.transitionTo(MA2); mA.update();
    CHECK(mA.getCurrentState() == MA2);
    CHECK(mB.getCurrentState() == MB1);
    mB.transitionTo(MB2); mB.update();
    CHECK(mA.getCurrentState() == MA2);
    CHECK(mB.getCurrentState() == MB2);
}

// =============================================================================
// FIXTURE 8: reentrancy — transitionTo() from inside entry()
// =============================================================================
static bool g_reentrantFired = false;
static void rs1Entry();   // forward-declare so RE_TABLE can reference it

enum RE : int8_t { RE_S0=0, RE_S1, RE_S2, RE_COUNT };
static constexpr PulseHSM::StaticState RE_TABLE[RE_COUNT] PULSEHSM_TABLE = {
    { PULSEHSM_NAME("S0"), nullptr, nullptr,  nullptr, 0, -1, nullptr, -1, -1 },
    { PULSEHSM_NAME("S1"), nullptr, rs1Entry, nullptr, 0, -1, nullptr, -1, -1 },
    { PULSEHSM_NAME("S2"), nullptr, nullptr,  nullptr, 0, -1, nullptr, -1, -1 },
};
PULSEHSM_VALIDATE_TABLE(RE_TABLE, RE_COUNT);
static PulseHSM reentrantFsm(RE_TABLE, RE_COUNT);

static void rs1Entry() {
    if (!g_reentrantFired) {
        g_reentrantFired = true;
        reentrantFsm.transitionTo(RE_S2);   // deferred by inTransition guard
    }
}

static void test_reentrant_transition() {
    g_reentrantFired = false;
    reentrantFsm.begin(RE_S0); reentrantFsm.update();
    reentrantFsm.transitionTo(RE_S1); reentrantFsm.update();
    reentrantFsm.update();   // apply the deferred transition
    CHECK(reentrantFsm.getCurrentState() == RE_S2);
}

// =============================================================================
// FIXTURE 9: timeout fires exactly once
// =============================================================================
static int tmEntryCount = 0;
static void tmBEntry() { tmEntryCount++; }

enum TM : int8_t { TM_A=0, TM_B, TM_COUNT };
static constexpr PulseHSM::StaticState TM_TABLE[TM_COUNT] PULSEHSM_TABLE = {
    { PULSEHSM_NAME("TM_A"), nullptr, nullptr,  nullptr, 1000, TM_B, nullptr, -1, -1 },
    { PULSEHSM_NAME("TM_B"), nullptr, tmBEntry, nullptr, 0,    -1,   nullptr, -1, -1 },
};
PULSEHSM_VALIDATE_TABLE(TM_TABLE, TM_COUNT);
static PulseHSM tmFsm(TM_TABLE, TM_COUNT);

static void test_timeout_fires_once() {
    tmEntryCount = 0;
    __pulsehsm_test_clock = 0;
    tmFsm.begin(TM_A);
    __pulsehsm_test_clock = 1000;
    for (int i = 0; i < 5; i++) tmFsm.update();
    CHECK(tmFsm.getCurrentState() == TM_B);
    CHECK(tmEntryCount == 1);
}

// =============================================================================
// main
// =============================================================================
int main() {
    printf("PulseHSM v2 tests (self-transition mode = %d)\n",
           PULSEHSM_SELF_TRANSITION_FULL_REINIT);

    test_begin_entry_order();
    test_sibling_transition_keeps_parent();
    test_event_bubbles_to_parent();
    test_self_transition_mode();
    test_self_transition_resets_timer();
    test_begin_composite_resolves_to_leaf();
    test_begin_out_of_range_fails();
    test_is_in_hierarchy_levels();
    test_event_payload();
    test_queue_fifo_order();
    test_queue_overflow_returns_false();
    test_dropped_events_saturate();
    test_initial_substate_begin();
    test_initial_substate_transition();
    test_initial_substate_nested();
    test_multi_instance_isolation();
    test_reentrant_transition();
    test_timeout_fires_once();

    if (g_failures == 0) { printf("ALL PASSED\n"); return 0; }
    printf("%d CHECK(s) FAILED\n", g_failures);
    return 1;
}
