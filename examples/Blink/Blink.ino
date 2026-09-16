/*
 * 01_Blink — minimal timed state machine
 *
 * LED_ON and LED_OFF alternate every 500 ms purely through per-state timeouts.
 * No events, no update callbacks — the classic blink loop as an FSM.
 *
 * The table is stored in flash on AVR (PULSEHSM_TABLE = PROGMEM) and in
 * .rodata on everything else — zero RAM cost for the state descriptor.
 *
 * Compatible: AVR, ESP32, RP2040, STM32, SAMD.
 */

#include "PulseHSM.h"

enum StateID : int8_t { ST_LED_ON = 0, ST_LED_OFF, ST_COUNT };

void ledOn()  { digitalWrite(LED_BUILTIN, HIGH); }
void ledOff() { digitalWrite(LED_BUILTIN, LOW);  }

//                         name                  update  entry    exit     ms   next         event   parent  initialChild
constexpr PulseHSM::StaticState TABLE[ST_COUNT] PULSEHSM_TABLE = {
    [ST_LED_ON]  = { PULSEHSM_NAME("LED_ON"),  nullptr, ledOn,  nullptr, 500, ST_LED_OFF, nullptr, -1, -1 },
    [ST_LED_OFF] = { PULSEHSM_NAME("LED_OFF"), nullptr, ledOff, nullptr, 500, ST_LED_ON,  nullptr, -1, -1 },
};
PULSEHSM_VALIDATE_TABLE(TABLE, ST_COUNT);

PulseHSM fsm(TABLE, ST_COUNT);

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    fsm.begin(ST_LED_ON);
}

void loop() {
    fsm.update();
}
