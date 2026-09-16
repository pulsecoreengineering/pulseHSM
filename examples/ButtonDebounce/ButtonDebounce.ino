// ButtonDebounce — debounce a physical button as a state machine.
//
// The raw pin is sampled in each state's update(). A change starts a short
// debounce window (a timed state); the press/release is only committed if the
// level is still there when the window expires. Demonstrates update() polling
// plus timed settling without extra timers.
//
// Compatible: AVR, ESP32, RP2040, STM32, SAMD.

#include "PulseHSM.h"

enum StateID : int8_t {
    ST_RELEASED = 0,
    ST_SETTLING_PRESS,
    ST_PRESSED,
    ST_SETTLING_RELEASE,
    ST_COUNT
};

const int BUTTON = 2;             // active-low to GND, INPUT_PULLUP
const unsigned long DEBOUNCE_MS = 25;

inline bool rawPressed() { return digitalRead(BUTTON) == LOW; }

// Forward-declare callbacks so their addresses compile into the table below.
void releasedUpdate();
void settlingPressUpdate();
void pressedEntry();
void pressedUpdate();
void settlingReleaseUpdate();

//                                name                    update                 entry        exit     ms           next                event   parent  initialChild
constexpr PulseHSM::StaticState TABLE[ST_COUNT] PULSEHSM_TABLE = {
    [ST_RELEASED]         = { PULSEHSM_NAME("released"),       releasedUpdate,       nullptr,      nullptr, 0,           -1,             nullptr, -1, -1 },
    [ST_SETTLING_PRESS]   = { PULSEHSM_NAME("settling_press"), settlingPressUpdate,  nullptr,      nullptr, DEBOUNCE_MS, ST_PRESSED,     nullptr, -1, -1 },
    [ST_PRESSED]          = { PULSEHSM_NAME("pressed"),        pressedUpdate,        pressedEntry, nullptr, 0,           -1,             nullptr, -1, -1 },
    [ST_SETTLING_RELEASE] = { PULSEHSM_NAME("settling_rel"),   settlingReleaseUpdate,nullptr,      nullptr, DEBOUNCE_MS, ST_RELEASED,    nullptr, -1, -1 },
};
PULSEHSM_VALIDATE_TABLE(TABLE, ST_COUNT);

PulseHSM fsm(TABLE, ST_COUNT);

// Callback implementations (fsm is fully constructed at this point).
void releasedUpdate()        { if ( rawPressed()) fsm.transitionTo(ST_SETTLING_PRESS);   }
void settlingPressUpdate()   { if (!rawPressed()) fsm.transitionTo(ST_RELEASED);          }
void pressedEntry()          { Serial.println(F("CLICK")); }
void pressedUpdate()         { if (!rawPressed()) fsm.transitionTo(ST_SETTLING_RELEASE);  }
void settlingReleaseUpdate() { if ( rawPressed()) fsm.transitionTo(ST_PRESSED);           }

void setup() {
    Serial.begin(115200);
    pinMode(BUTTON, INPUT_PULLUP);
    fsm.begin(ST_RELEASED);
}

void loop() {
    fsm.update();
}
