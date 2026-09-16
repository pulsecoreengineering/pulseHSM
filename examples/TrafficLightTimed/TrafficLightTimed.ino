/*
 * 03_TrafficLightTimed — chained timed transitions
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

enum StateID : int8_t { ST_RED = 0, ST_GREEN, ST_YELLOW, ST_COUNT };

void onRed();
void onGreen();
void onYellow();

//                                name             update  entry    exit     ms    next      event   parent  initialChild
constexpr PulseHSM::StaticState TABLE[ST_COUNT] PULSEHSM_TABLE = {
    [ST_RED]    = { PULSEHSM_NAME("RED"),    nullptr, onRed,    nullptr, 5000, ST_GREEN,  nullptr, -1, -1 },
    [ST_GREEN]  = { PULSEHSM_NAME("GREEN"),  nullptr, onGreen,  nullptr, 4000, ST_YELLOW, nullptr, -1, -1 },
    [ST_YELLOW] = { PULSEHSM_NAME("YELLOW"), nullptr, onYellow, nullptr, 1500, ST_RED,    nullptr, -1, -1 },
};
PULSEHSM_VALIDATE_TABLE(TABLE, ST_COUNT);

PulseHSM fsm(TABLE, ST_COUNT);

void allOff() {
    digitalWrite(PIN_RED,    LOW);
    digitalWrite(PIN_YELLOW, LOW);
    digitalWrite(PIN_GREEN,  LOW);
}

void onRed()    { allOff(); digitalWrite(PIN_RED,    HIGH); Serial.println("RED    — stop");    }
void onGreen()  { allOff(); digitalWrite(PIN_GREEN,  HIGH); Serial.println("GREEN  — go");      }
void onYellow() { allOff(); digitalWrite(PIN_YELLOW, HIGH); Serial.println("YELLOW — caution"); }

void setup() {
    Serial.begin(115200);
    pinMode(PIN_RED,    OUTPUT);
    pinMode(PIN_YELLOW, OUTPUT);
    pinMode(PIN_GREEN,  OUTPUT);
    fsm.begin(ST_RED);
}

void loop() {
    fsm.update();
}
