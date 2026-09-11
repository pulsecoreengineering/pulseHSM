/*
 * 05_HierarchicalController — full HSM with superstates
 *
 * Models an industrial machine controller with a three-level hierarchy:
 *
 *   SYSTEM  (superstate — E_STOP handler covers the whole machine)
 *   ├── STANDBY          wait for START command
 *   └── RUNNING          (superstate)
 *       ├── WARMING_UP   3-second warm-up, then auto-advances
 *       └── OPERATING    doing work; responds to STOP
 *
 * A single E_STOP handler on SYSTEM fires from any leaf state via bubbling.
 * setInitial(RUNNING, WARMING_UP) means "enter RUNNING → land in WARMING_UP".
 *
 * Concepts shown:
 *   - Nested superstates (parent parameter in addState)
 *   - setInitial()       : automatic resolution to default substate
 *   - Event bubbling     : unhandled events walk up to the parent
 *   - isInHierarchy()   : querying the active branch from outside
 *   - getPreviousName() : logging transitions
 *
 * Send commands via Serial monitor:
 *   t — START   (STANDBY → RUNNING → WARMING_UP)
 *   p — STOP    (OPERATING → STANDBY)
 *   e — E_STOP  (any state → STANDBY, via SYSTEM handler)
 *
 * Compatible: AVR, ESP32, RP2040, STM32, SAMD.
 */

#include "PulseHSM.h"

PulseHSM fsm;

enum Evt : uint8_t { EVT_START = 1, EVT_STOP, EVT_ESTOP };

int ST_SYSTEM, ST_STANDBY, ST_RUNNING, ST_WARMING_UP, ST_OPERATING;

// ── SYSTEM (superstate) ───────────────────────────────────

bool onEvent_System(uint8_t evt) {
  if (evt == EVT_ESTOP) {
    Serial.println("!!! E-STOP — returning to STANDBY");
    fsm.transitionTo(ST_STANDBY);
    return true;   // consumed — no further bubbling needed
  }
  return false;
}

// ── STANDBY ───────────────────────────────────────────────

void onEntry_Standby() {
  Serial.print("[STANDBY] (was: "); Serial.print(fsm.getPreviousName()); Serial.println(")");
  Serial.println("  Send 't' to start, 'e' for E-STOP.");
}

bool onEvent_Standby(uint8_t evt) {
  if (evt == EVT_START) {
    fsm.transitionTo(ST_RUNNING);   // resolves to WARMING_UP via setInitial
    return true;
  }
  return false;   // bubble EVT_ESTOP up to SYSTEM
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
  if (evt == EVT_STOP) {
    fsm.transitionTo(ST_STANDBY);
    return true;
  }
  return false;   // bubble EVT_ESTOP up through RUNNING → SYSTEM
}

// ── Setup & loop ──────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  // Parent must be added before its children.
  ST_SYSTEM     = fsm.addState("SYSTEM",     nullptr, nullptr,           nullptr, 0,    -1, onEvent_System,    -1);
  ST_STANDBY    = fsm.addState("STANDBY",    nullptr, onEntry_Standby,   nullptr, 0,    -1, onEvent_Standby,   ST_SYSTEM);
  ST_RUNNING    = fsm.addState("RUNNING",    nullptr, nullptr,           nullptr, 0,    -1, nullptr,           ST_SYSTEM);
  ST_WARMING_UP = fsm.addState("WARMING_UP", nullptr, onEntry_WarmingUp, nullptr, 3000, -1, nullptr,           ST_RUNNING);
  ST_OPERATING  = fsm.addState("OPERATING",  nullptr, onEntry_Operating, nullptr, 0,    -1, onEvent_Operating, ST_RUNNING);

  fsm.setInitial(ST_RUNNING, ST_WARMING_UP);   // RUNNING → always starts in WARMING_UP
  // Wire WARMING_UP timeout to OPERATING now that both indices are known
  // (use transitionTo in an update callback instead — cleaner for chained siblings)

  fsm.begin(ST_STANDBY);
}

// Wire WARMING_UP → OPERATING via update (avoids needing the index at addState time)
void loop() {
  // After warming up, advance to OPERATING
  if (fsm.getCurrentState() == ST_WARMING_UP &&
      fsm.getStateElapsed() >= 3000) {
    fsm.transitionTo(ST_OPERATING);
  }

  if (Serial.available()) {
    char ch = Serial.read();
    if (ch == 't') fsm.sendEvent(EVT_START);
    if (ch == 'p') fsm.sendEvent(EVT_STOP);
    if (ch == 'e') fsm.sendEvent(EVT_ESTOP);
  }

  fsm.update();
}
