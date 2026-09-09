/*
 * 03_TrafficLight — chained timed transitions
 *
 * Classic traffic light: RED → GREEN → YELLOW → RED, driven entirely
 * by per-state timeouts. No events, no update callbacks — just timers.
 *
 * Concepts shown:
 *   - Chained timeoutMs / timeoutNext across multiple states
 *   - entry callbacks for output (LEDs + Serial)
 *   - getCurrentName() : reading the active state name outside the machine
 *
 * Wiring (optional — Serial output works without LEDs):
 *   PIN_RED    → red    LED → 220 Ω → GND
 *   PIN_YELLOW → yellow LED → 220 Ω → GND
 *   PIN_GREEN  → green  LED → 220 Ω → GND
 *
 * Compatible: AVR, ESP32, RP2040, STM32, SAMD.
 */

#include "PulseHSM.h"

#define PIN_RED    3
#define PIN_YELLOW 5
#define PIN_GREEN  6

PulseHSM fsm;

void allOff() {
  digitalWrite(PIN_RED,    LOW);
  digitalWrite(PIN_YELLOW, LOW);
  digitalWrite(PIN_GREEN,  LOW);
}

void onRed() {
  allOff();
  digitalWrite(PIN_RED, HIGH);
  Serial.println("RED    — stop");
}

void onGreen() {
  allOff();
  digitalWrite(PIN_GREEN, HIGH);
  Serial.println("GREEN  — go");
}

void onYellow() {
  allOff();
  digitalWrite(PIN_YELLOW, HIGH);
  Serial.println("YELLOW — caution");
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_RED,    OUTPUT);
  pinMode(PIN_YELLOW, OUTPUT);
  pinMode(PIN_GREEN,  OUTPUT);

  // Indices: RED=0, GREEN=1, YELLOW=2
  //                   name       update  entry     exit     ms    next  event
  fsm.addState("RED",    nullptr, onRed,    nullptr, 5000,  1, nullptr); // 5 s red
  fsm.addState("GREEN",  nullptr, onGreen,  nullptr, 4000,  2, nullptr); // 4 s green
  fsm.addState("YELLOW", nullptr, onYellow, nullptr, 1500,  0, nullptr); // 1.5 s yellow

  fsm.begin(0); // start RED
}

void loop() {
  fsm.update();
}
