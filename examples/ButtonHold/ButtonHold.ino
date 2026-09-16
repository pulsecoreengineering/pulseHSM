/*
 * 02_ButtonHold — event-driven button with hold detection
 *
 * Three states driven by button presses polled in loop().
 * A short press prints "CLICK". Holding for 1 second escalates to HELD,
 * using the state's built-in timeout rather than a manual update callback.
 *
 * Concepts shown:
 *   - sendEvent()      : posting an event from application code
 *   - onEvent callback : handling events and returning true to consume them
 *   - timeoutMs/Next   : built-in 1-second hold escalation
 *   - transitionTo()   : requesting a state change from inside a callback
 *
 * Wiring: button between BTN_PIN and GND (uses internal pull-up).
 * Compatible: AVR, ESP32, RP2040, STM32, SAMD.
 */

#include "PulseHSM.h"

#define BTN_PIN 4

enum StateID : int8_t { ST_IDLE = 0, ST_PRESSED, ST_HELD, ST_COUNT };
enum Evt     : uint8_t { EVT_PRESS = 1, EVT_RELEASE };

// Forward declarations
bool onIdle(uint8_t evt);
bool onPressed(uint8_t evt);
void onEntryHeld();
bool onHeld(uint8_t evt);

//                                name               update  entry       exit  ms    next      event     parent  initialChild
constexpr PulseHSM::StaticState TABLE[ST_COUNT] PULSEHSM_TABLE = {
    [ST_IDLE]    = { PULSEHSM_NAME("IDLE"),    nullptr, nullptr,    nullptr,  0,    -1,       onIdle,   -1, -1 },
    [ST_PRESSED] = { PULSEHSM_NAME("PRESSED"), nullptr, nullptr,    nullptr,  1000, ST_HELD,  onPressed,-1, -1 },
    [ST_HELD]    = { PULSEHSM_NAME("HELD"),    nullptr, onEntryHeld,nullptr,  0,    -1,       onHeld,   -1, -1 },
};
PULSEHSM_VALIDATE_TABLE(TABLE, ST_COUNT);

PulseHSM fsm(TABLE, ST_COUNT);

bool onIdle(uint8_t evt) {
    if (evt == EVT_PRESS) { fsm.transitionTo(ST_PRESSED); return true; }
    return false;
}

bool onPressed(uint8_t evt) {
    if (evt == EVT_RELEASE) { Serial.println("CLICK"); fsm.transitionTo(ST_IDLE); return true; }
    return false;
}

void onEntryHeld() { Serial.println("HELD"); }

bool onHeld(uint8_t evt) {
    if (evt == EVT_RELEASE) { fsm.transitionTo(ST_IDLE); return true; }
    return false;
}

void setup() {
    Serial.begin(115200);
    pinMode(BTN_PIN, INPUT_PULLUP);
    fsm.begin(ST_IDLE);
    Serial.println("Ready — press the button.");
}

void loop() {
    static bool lastState = HIGH;
    bool current = digitalRead(BTN_PIN);
    if (current != lastState) {
        lastState = current;
        fsm.sendEvent(current == LOW ? EVT_PRESS : EVT_RELEASE);
    }
    fsm.update();
}
