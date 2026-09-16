/*
 * 05_HierarchicalController — full HSM with superstates
 *
 * Models an industrial machine controller with a three-level hierarchy:
 *
 *   SYSTEM  (superstate — E_STOP handler covers the whole machine)
 *   ├── STANDBY          wait for START command
 *   └── RUNNING          (superstate)
 *       ├── WARMING_UP   3-second warm-up, then auto-advances to OPERATING
 *       └── OPERATING    doing work; responds to STOP
 *
 * A single E_STOP handler on SYSTEM fires from any leaf state via bubbling.
 * RUNNING's initialChild = WARMING_UP: transitioning to RUNNING always lands
 * in WARMING_UP automatically.
 *
 * Concepts shown:
 *   - Nested superstates (parent field in StaticState)
 *   - initialChild      : automatic resolution to default substate
 *   - Event bubbling    : unhandled events walk up to the parent
 *   - isInHierarchy()   : querying the active branch from outside
 *   - getPreviousName() : logging transitions
 *   - PULSEHSM_VALIDATE_TABLE: structural errors become compiler errors
 *
 * Send commands via Serial monitor:
 *   t — START   (STANDBY → RUNNING → WARMING_UP)
 *   p — STOP    (OPERATING → STANDBY)
 *   e — E_STOP  (any state → STANDBY, via SYSTEM handler)
 *
 * The `[ST_x] = {...}` designated-initialiser form is a GNU extension accepted
 * by avr-g++ and the ESP32/RP2040 toolchains under their default -std=gnu++11/17.
 *
 * Compatible: AVR, ESP32, RP2040, STM32, SAMD.
 */

#include "PulseHSM.h"

enum StateID : int8_t {
    ST_SYSTEM = 0,
    ST_STANDBY,
    ST_RUNNING,
    ST_WARMING_UP,
    ST_OPERATING,
    STATE_COUNT
};

enum Evt : uint8_t { EVT_START = 1, EVT_STOP, EVT_ESTOP };

// Forward declarations
bool onEvent_System(uint8_t evt);
void onEntry_Standby();
bool onEvent_Standby(uint8_t evt);
void onEntry_WarmingUp();
void onEntry_Operating();
bool onEvent_Operating(uint8_t evt);

// [ID] = { Name, Update, Entry, Exit, TimeoutMs, TimeoutNext, EventCallback, Parent, InitialChild }
constexpr PulseHSM::StaticState HSM_STATE_TABLE[STATE_COUNT] PULSEHSM_TABLE = {
    [ST_SYSTEM]     = { PULSEHSM_NAME("SYSTEM"),     nullptr, nullptr,           nullptr, 0,    -1,          onEvent_System,    -1,        ST_STANDBY   },
    [ST_STANDBY]    = { PULSEHSM_NAME("STANDBY"),    nullptr, onEntry_Standby,   nullptr, 0,    -1,          onEvent_Standby,   ST_SYSTEM, -1           },
    [ST_RUNNING]    = { PULSEHSM_NAME("RUNNING"),    nullptr, nullptr,           nullptr, 0,    -1,          nullptr,           ST_SYSTEM, ST_WARMING_UP},
    [ST_WARMING_UP] = { PULSEHSM_NAME("WARMING_UP"), nullptr, onEntry_WarmingUp, nullptr, 3000, ST_OPERATING,nullptr,           ST_RUNNING,-1           },
    [ST_OPERATING]  = { PULSEHSM_NAME("OPERATING"),  nullptr, onEntry_Operating, nullptr, 0,    -1,          onEvent_Operating, ST_RUNNING,-1           },
};
PULSEHSM_VALIDATE_TABLE(HSM_STATE_TABLE, STATE_COUNT);

PulseHSM fsm(HSM_STATE_TABLE, STATE_COUNT);

// ── SYSTEM (superstate) ───────────────────────────────────

bool onEvent_System(uint8_t evt) {
    if (evt == EVT_ESTOP) {
        Serial.println("!!! E-STOP — returning to STANDBY");
        fsm.transitionTo(ST_STANDBY);
        return true;
    }
    return false;
}

// ── STANDBY ───────────────────────────────────────────────

void onEntry_Standby() {
    Serial.print("[STANDBY] (was: "); Serial.print(fsm.getPreviousName()); Serial.println(")");
    Serial.println("  Send 't' to start, 'e' for E-STOP.");
}

bool onEvent_Standby(uint8_t evt) {
    if (evt == EVT_START) { fsm.transitionTo(ST_RUNNING); return true; }
    return false;
}

// ── WARMING_UP ────────────────────────────────────────────

void onEntry_WarmingUp() {
    Serial.println("[WARMING_UP] Heating up — 3 seconds...");
}

// ── OPERATING ─────────────────────────────────────────────

void onEntry_Operating() {
    Serial.println("[OPERATING] Running. Send 'p' to stop, 'e' for E-STOP.");
}

bool onEvent_Operating(uint8_t evt) {
    if (evt == EVT_STOP) { fsm.transitionTo(ST_STANDBY); return true; }
    return false;
}

// ── Setup & loop ──────────────────────────────────────────

void setup() {
    Serial.begin(115200);
    while (!Serial) {}
    fsm.begin(ST_STANDBY);
}

void loop() {
    if (Serial.available()) {
        char ch = Serial.read();
        if (ch == 't') fsm.sendEvent(EVT_START);
        if (ch == 'p') fsm.sendEvent(EVT_STOP);
        if (ch == 'e') fsm.sendEvent(EVT_ESTOP);
    }
    fsm.update();
}
