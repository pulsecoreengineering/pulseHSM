// Thermostat — demonstrates event payloads via sendEvent(evt, data) / getEventData().
//
// A SET_TARGET command carries the desired temperature as its int32 payload.
// Temperatures are in tenths of a degree C (e.g. 215 = 21.5 C) to stay integer.
// Type a number + Enter in the Serial Monitor to set a new target.
//
//   IDLE   -- temp < target - hysteresis --> HEATING
//   HEATING-- temp >= target             --> IDLE
//   (COOLING omitted for brevity; same shape as HEATING)

#include "PulseHSM.h"

enum { ST_IDLE, ST_HEATING };

PulseHSM fsm;
const uint8_t EVT_SET_TARGET = 1;
const int32_t HYSTERESIS = 5;          // 0.5 C

int32_t targetTemp  = 210;             // 21.0 C
int32_t currentTemp = 180;             // 18.0 C (simulated)

// Any handler can read the payload of the event it's dispatching.
bool onSetTarget(uint8_t e) {
    if (e == EVT_SET_TARGET) {
        targetTemp = fsm.getEventData();
        Serial.print(F("New target: ")); Serial.println(targetTemp);
        return true;
    }
    return false;
}

void idleUpdate() {
    currentTemp -= 1;                                   // simulate slow cooling
    if (currentTemp < targetTemp - HYSTERESIS) fsm.transitionTo(ST_HEATING);
}
void heatingEntry()  { Serial.println(F("HEATING")); }
void heatingUpdate() {
    currentTemp += 2;                                   // simulate heating
    if (currentTemp >= targetTemp) { Serial.println(F("target reached")); fsm.transitionTo(ST_IDLE); }
}

void setup() {
    Serial.begin(115200);
    //           name      update        entry         exit     timeoutMs next     onEvent      parent
    fsm.addState("idle",    idleUpdate,   nullptr,      nullptr, 0,        -1,     onSetTarget, -1);
    fsm.addState("heating", heatingUpdate,heatingEntry, nullptr, 0,        -1,     onSetTarget, -1);
    fsm.begin(ST_IDLE);
}

void loop() {
    fsm.update();
    delay(200);

    // Parse a typed integer as the new target and deliver it as a payload.
    static int32_t acc = 0; static bool have = false;
    while (Serial.available()) {
        int c = Serial.read();
        if (c >= '0' && c <= '9') { acc = acc * 10 + (c - '0'); have = true; }
        else if (have) { fsm.sendEvent(EVT_SET_TARGET, acc); acc = 0; have = false; }
    }
}
