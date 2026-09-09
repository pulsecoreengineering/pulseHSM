/*
 * 04_VendingMachine — events with payloads
 *
 * A coin-operated vending machine simulated over Serial.
 * Send characters from the Serial monitor to interact:
 *
 *   c  — insert a coin  (25 cents each)
 *   s  — select product (costs 75 cents)
 *   r  — cancel / refund
 *
 * Concepts shown:
 *   - sendEvent() with an int32_t payload
 *   - getEventData() : reading the payload inside onEvent
 *   - Multiple events dispatched to the same handler
 *   - State-local variables via statics in callbacks
 *
 * Compatible: AVR, ESP32, RP2040, STM32, SAMD.
 */

#include "PulseHSM.h"

#define COIN_VALUE   25   // cents per coin
#define ITEM_PRICE   75   // cents

PulseHSM fsm;

enum Evt : uint8_t { EVT_COIN = 1, EVT_SELECT, EVT_CANCEL };

int ST_IDLE, ST_COLLECTING, ST_DISPENSING;

static int balance = 0;   // cents inserted so far

// ── IDLE ─────────────────────────────────────────────────

void onEntry_Idle() {
  balance = 0;
  Serial.println("\n[IDLE] Insert coins (c) to start.");
}

bool onEvent_Idle(uint8_t evt) {
  if (evt == EVT_COIN) {
    balance += fsm.getEventData();
    fsm.transitionTo(ST_COLLECTING);
    return true;
  }
  return false;
}

// ── COLLECTING ────────────────────────────────────────────

void onEntry_Collecting() {
  Serial.print("[COLLECTING] Balance: ");
  Serial.print(balance);
  Serial.println(" ¢  — insert more (c), select (s), or cancel (r).");
}

bool onEvent_Collecting(uint8_t evt) {
  if (evt == EVT_COIN) {
    balance += fsm.getEventData();
    Serial.print("  + coin  →  ");
    Serial.print(balance);
    Serial.println(" ¢");
    if (balance >= ITEM_PRICE) {
      Serial.println("  Enough credit! Press (s) to dispense.");
    }
    return true;
  }
  if (evt == EVT_SELECT) {
    if (balance >= ITEM_PRICE) {
      fsm.transitionTo(ST_DISPENSING);
    } else {
      Serial.print("  Need ");
      Serial.print(ITEM_PRICE - balance);
      Serial.println(" ¢ more.");
    }
    return true;
  }
  if (evt == EVT_CANCEL) {
    Serial.print("  Refunding ");
    Serial.print(balance);
    Serial.println(" ¢.");
    fsm.transitionTo(ST_IDLE);
    return true;
  }
  return false;
}

// ── DISPENSING ────────────────────────────────────────────

void onEntry_Dispensing() {
  int change = balance - ITEM_PRICE;
  Serial.print("[DISPENSING] Vending item");
  if (change > 0) { Serial.print(" + "); Serial.print(change); Serial.print(" ¢ change"); }
  Serial.println("... (returns to IDLE in 2 s)");
}

// ── Setup & loop ──────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  while (!Serial) {}   // wait for USB Serial on boards that need it

  ST_IDLE       = fsm.addState("IDLE",       nullptr, onEntry_Idle,       nullptr, 0,    -1, onEvent_Idle);
  ST_COLLECTING = fsm.addState("COLLECTING", nullptr, onEntry_Collecting, nullptr, 0,    -1, onEvent_Collecting);
  ST_DISPENSING = fsm.addState("DISPENSING", nullptr, onEntry_Dispensing, nullptr, 2000, ST_IDLE, nullptr);

  fsm.begin(ST_IDLE);
}

void loop() {
  if (Serial.available()) {
    char ch = Serial.read();
    if (ch == 'c') fsm.sendEvent(EVT_COIN,   COIN_VALUE);
    if (ch == 's') fsm.sendEvent(EVT_SELECT,  0);
    if (ch == 'r') fsm.sendEvent(EVT_CANCEL,  0);
  }
  fsm.update();
}
