// InterruptButton — inject events from an ISR into PulseHSM's interrupt-safe queue.
//
// A falling edge on the button pin fires an interrupt whose handler calls
// sendEvent(). The machine processes it in loop() at a safe time. This is what
// the queue's critical-section guarding is for: the ISR and update() can touch
// the queue concurrently without corrupting it.
//
// IRAM_ATTR is required for ISRs on ESP32; on AVR it's harmlessly empty.
// Compatible: AVR, ESP32, RP2040, STM32, SAMD.

#include "PulseHSM.h"

#ifndef IRAM_ATTR
#define IRAM_ATTR
#endif

enum StateID : uint8_t { ST_WAITING = 0, ST_TRIGGERED, ST_COUNT };
enum Evt     : uint8_t { EVT_PRESS = 1 };

const int BUTTON = 2;

void waitingEntry();
bool waitingEvent(uint8_t e);
void triggeredEntry();

//                                name              update  entry          exit     ms    next        event       parent  initialChild
constexpr PulseHSM::StaticState TABLE[ST_COUNT] PULSEHSM_TABLE = {
    [ST_WAITING]   = { PULSEHSM_NAME("waiting"),   nullptr, waitingEntry,   nullptr, 0,    -1,         waitingEvent,-1, -1 },
    [ST_TRIGGERED] = { PULSEHSM_NAME("triggered"), nullptr, triggeredEntry, nullptr, 1000, ST_WAITING, nullptr,     -1, -1 },
};
PULSEHSM_VALIDATE_TABLE(TABLE, ST_COUNT);

PulseHSM fsm(TABLE, ST_COUNT);

void waitingEntry()   { Serial.println(F("WAITING for button...")); }
void triggeredEntry() { Serial.println(F("TRIGGERED")); }
bool waitingEvent(uint8_t e) {
    if (e == EVT_PRESS) { fsm.transitionTo(ST_TRIGGERED); return true; }
    return false;
}

// Keep ISRs tiny: just enqueue. All real work happens in the state handlers.
void IRAM_ATTR onButtonFall() { fsm.sendEvent(EVT_PRESS); }

void setup() {
    Serial.begin(115200);
    pinMode(BUTTON, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON), onButtonFall, FALLING);
    fsm.begin(ST_WAITING);
}

void loop() {
    fsm.update();
}
