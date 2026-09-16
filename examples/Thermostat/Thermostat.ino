// Thermostat — demonstrates event payloads via sendEvent(evt, data) / getEventData().
//
// A SET_TARGET command carries the desired temperature as its int32 payload.
// Temperatures are in tenths of a degree C (e.g. 215 = 21.5 C) to stay integer.
// Type a number + Enter in the Serial Monitor to set a new target.
//
//   IDLE    -- temp < target - hysteresis --> HEATING
//   HEATING -- temp >= target             --> IDLE
//
// Compatible: AVR, ESP32, RP2040, STM32, SAMD.

#include "PulseHSM.h"

enum StateID : int8_t { ST_IDLE = 0, ST_HEATING, ST_COUNT };
enum Evt     : uint8_t { EVT_SET_TARGET = 1 };

const int32_t HYSTERESIS = 5;    // 0.5 C

int32_t targetTemp  = 210;       // 21.0 C
int32_t currentTemp = 180;       // 18.0 C (simulated)

// Forward declarations
bool onSetTarget(uint8_t e);
void idleUpdate();
void heatingEntry();
void heatingUpdate();

//                                name            update        entry        exit     ms   next  event        parent  initialChild
constexpr PulseHSM::StaticState TABLE[ST_COUNT] PULSEHSM_TABLE = {
    [ST_IDLE]    = { PULSEHSM_NAME("idle"),    idleUpdate,   nullptr,     nullptr, 0,   -1,   onSetTarget, -1, -1 },
    [ST_HEATING] = { PULSEHSM_NAME("heating"), heatingUpdate,heatingEntry,nullptr, 0,   -1,   onSetTarget, -1, -1 },
};
PULSEHSM_VALIDATE_TABLE(TABLE, ST_COUNT);

PulseHSM fsm(TABLE, ST_COUNT);

bool onSetTarget(uint8_t e) {
    if (e == EVT_SET_TARGET) {
        targetTemp = fsm.getEventData();
        Serial.print(F("New target: ")); Serial.println(targetTemp);
        return true;
    }
    return false;
}

void idleUpdate() {
    currentTemp -= 1;
    if (currentTemp < targetTemp - HYSTERESIS) fsm.transitionTo(ST_HEATING);
}

void heatingEntry() { Serial.println(F("HEATING")); }

void heatingUpdate() {
    currentTemp += 2;
    if (currentTemp >= targetTemp) { Serial.println(F("target reached")); fsm.transitionTo(ST_IDLE); }
}

void setup() {
    Serial.begin(115200);
    fsm.begin(ST_IDLE);
}

void loop() {
    fsm.update();
    delay(200);

    static int32_t acc = 0; static bool have = false;
    while (Serial.available()) {
        int c = Serial.read();
        if (c >= '0' && c <= '9') { acc = acc * 10 + (c - '0'); have = true; }
        else if (have) { fsm.sendEvent(EVT_SET_TARGET, acc); acc = 0; have = false; }
    }
}
