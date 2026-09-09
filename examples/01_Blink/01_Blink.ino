/*
 * 01_Blink — PulseHSM getting started
 *
 * The simplest possible HSM: two states that blink the built-in LED.
 * Each state has a 500 ms timeout that transitions to the other state.
 *
 * Concepts shown:
 *   - addState()  : registering states with timeouts
 *   - timeoutMs   : how long to stay in this state
 *   - timeoutNext : which state to go to when the timer fires
 *   - begin()     : starting the machine
 *   - update()    : the main tick — call once per loop()
 *
 * Compatible: AVR, ESP32, RP2040, STM32, SAMD, and any Arduino board.
 */

#include "PulseHSM.h"

// ESP32 Arduino core 3.x does not define LED_BUILTIN on all boards.
// Change this to the correct GPIO number for your board.
#ifndef LED_BUILTIN
  #define LED_BUILTIN 2
#endif

PulseHSM fsm;

void ledOn()  { digitalWrite(LED_BUILTIN, HIGH); }
void ledOff() { digitalWrite(LED_BUILTIN, LOW);  }

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);

  // States are indexed in the order they are added: 0, 1, 2 ...
  // addState(name, update, entry, exit, timeoutMs, timeoutNext, onEvent, parent)
  fsm.addState("LED_ON",  nullptr, ledOn,  nullptr, 500, 1, nullptr); // index 0 → times out to 1
  fsm.addState("LED_OFF", nullptr, ledOff, nullptr, 500, 0, nullptr); // index 1 → times out to 0

  fsm.begin(0); // start in LED_ON
}

void loop() {
  fsm.update();
}
