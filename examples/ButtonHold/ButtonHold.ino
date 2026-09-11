/*
 * 02_Button — event-driven button with hold detection
 *
 * Three states driven by button presses polled in loop().
 * A short press prints "CLICK". Holding for 1 second prints "HELD".
 *
 * Concepts shown:
 *   - sendEvent()      : posting an event from application code
 *   - onEvent callback : handling events and returning true to consume them
 *   - getStateElapsed(): how long the machine has been in the current state
 *   - transitionTo()   : requesting a state change from inside a callback
 *
 * Wiring: button between BTN_PIN and GND (uses internal pull-up).
 * Compatible: AVR, ESP32, RP2040, STM32, SAMD.
 */

#include "PulseHSM.h"

#define BTN_PIN 4   // change to match your board

PulseHSM fsm;

enum Event : uint8_t { EVT_PRESS = 1, EVT_RELEASE };

int ST_IDLE, ST_PRESSED, ST_HELD;

// ── Event handlers ────────────────────────────────────────

bool onIdle(uint8_t evt) {
  if (evt == EVT_PRESS) { fsm.transitionTo(ST_PRESSED); return true; }
  return false;
}

bool onPressed(uint8_t evt) {
  if (evt == EVT_RELEASE) {
    Serial.println("CLICK");
    fsm.transitionTo(ST_IDLE);
    return true;
  }
  return false;
}

bool onHeld(uint8_t evt) {
  if (evt == EVT_RELEASE) { fsm.transitionTo(ST_IDLE); return true; }
  return false;
}

// ── Update: escalate PRESSED → HELD after 1 second ───────

void updatePressed() {
  if (fsm.getStateElapsed() >= 1000) {
    Serial.println("HELD");
    fsm.transitionTo(ST_HELD);
  }
}

// ── Setup & loop ──────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  pinMode(BTN_PIN, INPUT_PULLUP);

  ST_IDLE    = fsm.addState("IDLE",    nullptr,      nullptr, nullptr, 0, -1, onIdle);
  ST_PRESSED = fsm.addState("PRESSED", updatePressed, nullptr, nullptr, 0, -1, onPressed);
  ST_HELD    = fsm.addState("HELD",   nullptr,       nullptr, nullptr, 0, -1, onHeld);

  fsm.begin(ST_IDLE);
  Serial.println("Ready — press the button.");
}

void loop() {
  // Poll button and fire events on edge transitions
  static bool lastState = HIGH;
  bool current = digitalRead(BTN_PIN);
  if (current != lastState) {
    lastState = current;
    fsm.sendEvent(current == LOW ? EVT_PRESS : EVT_RELEASE);
  }

  fsm.update();
}
