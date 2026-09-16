// TrafficLight — a flat timed state machine (the classic FSM).
//
// RED -> GREEN -> YELLOW -> RED on timers. Pressing the pedestrian button
// (send "1") while GREEN cuts the green short and goes to YELLOW early — an
// event pre-empting a timed transition.
//
// Compatible: AVR, ESP32, RP2040, STM32, SAMD.

#include "PulseHSM.h"

enum StateID : int8_t { ST_RED = 0, ST_GREEN, ST_YELLOW, ST_COUNT };
enum Evt     : uint8_t { EVT_WALK = 1 };

void redEntry();
void greenEntry();
void yellowEntry();
bool greenEvent(uint8_t e);

//                                name             update  entry       exit     ms    next      event       parent  initialChild
constexpr PulseHSM::StaticState TABLE[ST_COUNT] PULSEHSM_TABLE = {
    [ST_RED]    = { PULSEHSM_NAME("red"),    nullptr, redEntry,    nullptr, 5000, ST_GREEN,  nullptr,    -1, -1 },
    [ST_GREEN]  = { PULSEHSM_NAME("green"),  nullptr, greenEntry,  nullptr, 5000, ST_YELLOW, greenEvent, -1, -1 },
    [ST_YELLOW] = { PULSEHSM_NAME("yellow"), nullptr, yellowEntry, nullptr, 2000, ST_RED,    nullptr,    -1, -1 },
};
PULSEHSM_VALIDATE_TABLE(TABLE, ST_COUNT);

PulseHSM fsm(TABLE, ST_COUNT);

void redEntry()    { Serial.println(F("RED")); }
void greenEntry()  { Serial.println(F("GREEN")); }
void yellowEntry() { Serial.println(F("YELLOW")); }

bool greenEvent(uint8_t e) {
    if (e == EVT_WALK) {
        Serial.println(F("  walk requested -> ending green"));
        fsm.transitionTo(ST_YELLOW);
        return true;
    }
    return false;
}

void setup() {
    Serial.begin(115200);
    fsm.begin(ST_RED);
}

void loop() {
    fsm.update();
    if (Serial.available() && Serial.read() == '1') fsm.sendEvent(EVT_WALK);
}
