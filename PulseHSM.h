#ifndef PULSE_HSM_H
#define PULSE_HSM_H

#include <Arduino.h>

// ============================================================================
// Compile-time configuration
// Override any of these with -D flags or a #define BEFORE #include "PulseHSM.h".
// ============================================================================
#ifndef PULSEHSM_MAX_STATES
#define PULSEHSM_MAX_STATES 8
#endif
#ifndef PULSEHSM_MAX_EVENTS
#define PULSEHSM_MAX_EVENTS 8      // MUST be a power of two (ring buffer uses bitmask indexing)
#endif
#ifndef PULSEHSM_MAX_DEPTH
#define PULSEHSM_MAX_DEPTH 4       // max number of ANCESTORS a leaf may have
#endif

// Self-transition behaviour: transitionTo(currentState) while already in it.
//   0 = lightweight  — only entryTime is reset; entry()/exit() do NOT run (DEFAULT)
//   1 = full reinit  — exit() then entry() run for THAT state only (ancestors untouched)
#ifndef PULSEHSM_SELF_TRANSITION_FULL_REINIT
#define PULSEHSM_SELF_TRANSITION_FULL_REINIT 0
#endif

// ---- Configuration sanity checks (fail loudly at compile time, not silently) ----
static_assert(PULSEHSM_MAX_EVENTS > 0 &&
              (PULSEHSM_MAX_EVENTS & (PULSEHSM_MAX_EVENTS - 1)) == 0,
              "PULSEHSM_MAX_EVENTS must be a power of two (2, 4, 8, 16, ...). "
              "The event ring buffer indexes with a bitmask, so non-power-of-two "
              "sizes corrupt the queue.");
static_assert(PULSEHSM_MAX_STATES > 0 && PULSEHSM_MAX_STATES <= 127,
              "PULSEHSM_MAX_STATES must be 1..127 (state indices are stored as int8_t).");
static_assert(PULSEHSM_MAX_DEPTH >= 1,
              "PULSEHSM_MAX_DEPTH must be >= 1.");

// ============================================================================
// Interrupt-safe critical section (RAII). Save/restore where the platform
// supports it, so sendEvent() may be called from an ISR without prematurely
// re-enabling interrupts.
//   AVR / ARM Cortex-M : proper save + restore (nesting-safe).
//   ESP32 / other      : same-core interrupt lock. Producers on the OTHER core
//                        of a dual-core chip need their own synchronisation.
// ============================================================================
class PulseHSMCritical {
#if defined(__AVR__)
    uint8_t s_;
  public:
    PulseHSMCritical() : s_(SREG) { cli(); }
    ~PulseHSMCritical() { SREG = s_; }
#elif defined(ARDUINO_ARCH_RP2040)
    // arduino-pico (earlephilhower) exposes Pico SDK save/restore helpers but
    // does NOT include CMSIS headers, so __get_PRIMASK / __disable_irq are absent.
    uint32_t s_;
  public:
    PulseHSMCritical() : s_(save_and_disable_interrupts()) {}
    ~PulseHSMCritical() { restore_interrupts(s_); }
#elif defined(__CORTEX_M) || defined(ARDUINO_ARCH_SAMD) || \
      defined(ARDUINO_ARCH_STM32) || defined(ARDUINO_ARCH_NRF52) || defined(TEENSYDUINO)
    uint32_t pri_;
  public:
    PulseHSMCritical() : pri_(__get_PRIMASK()) { __disable_irq(); }
    ~PulseHSMCritical() { if (!pri_) __enable_irq(); }
#else
  public:
    PulseHSMCritical() { noInterrupts(); }
    ~PulseHSMCritical() { interrupts(); }
#endif
    PulseHSMCritical(const PulseHSMCritical&) = delete;
    PulseHSMCritical& operator=(const PulseHSMCritical&) = delete;
};

class PulseHSM {
public:
    using Action  = void (*)();
    using EventCb = bool (*)(uint8_t event);   // return true = handled, false = bubble up

    PulseHSM();

    // Add a state. Returns its index, or -1 if the table is full OR the state's
    // depth would exceed PULSEHSM_MAX_DEPTH. A parent must be added before its
    // children (its index is what you pass as `parent`).
    int addState(const char* name,
                 Action update,
                 Action entry,
                 Action exit,
                 unsigned long timeoutMs,
                 int timeoutNext,
                 EventCb onEvent,
                 int parent = -1);

    // Mark `child` as the default substate entered when `parent` is targeted.
    // `child` must be a direct child of `parent`. Returns false on bad indices.
    // Once set, transitionTo(parent) and begin(parent) resolve to the deepest
    // initial leaf before entering.
    bool setInitial(int parent, int child);

    // Start the machine. startState may be a leaf or a composite that has an
    // initial substate set (recursively). Returns false if startState is invalid
    // or is a composite with no initial substate configured.
    bool begin(int startState);

    // Run the scheduler — call once per loop().
    void update();

    // Request a transition (applied at the end of the current update()).
    void transitionTo(int newState);

    // Enqueue an event (interrupt-safe ring buffer).
    // Optional int32 payload — read it inside an onEvent handler via getEventData().
    // Returns true if the event was queued, false if the queue was full (event dropped).
    bool sendEvent(uint8_t event, int32_t data = 0);

    // Getters
    int getCurrentState() const;
    const char* getStateName(int idx) const;
    const char* getCurrentName() const;
    unsigned long getStateElapsed() const;
    bool isInHierarchy(int state) const;   // true if `state` is the current state or an active ancestor

    // Payload of the event currently being dispatched (valid inside onEvent handlers).
    int32_t getEventData() const;

    // Where we came from (valid in entry/exit/update of the new state).
    int getPreviousState() const;
    const char* getPreviousName() const;

private:
    struct State {
        const char* name;
        Action update;
        Action entry;
        Action exit;
        unsigned long timeoutMs;
        int timeoutNext;
        EventCb onEvent;
        int8_t parent;
        int8_t initialChild;   // -1 = leaf / no default child
    };
    State states[PULSEHSM_MAX_STATES];
    int stateCount;
    int currentState;
    int previousState;
    int pendingState;
    unsigned long entryTime;
    uint8_t evtQueue[PULSEHSM_MAX_EVENTS];
    int32_t evtData[PULSEHSM_MAX_EVENTS];   // parallel payload ring
    uint8_t evtHead;
    uint8_t evtCount;
    int32_t currentEventData;               // payload of the event being dispatched
    bool inTransition;

    void _callEntryChain(int state, int stopAt = -1);
    void _callExitChain(int state, int stopAt = -1);
    void _dispatchEvent(uint8_t evt);
    void _runUpdates();
    void _executeTransition(int toState);
    int  _findLCA(int a, int b) const;
    bool _isLeaf(int state) const;
    int  _resolveEntry(int s) const;   // walk initialChild chain to deepest leaf
};

#endif // PULSE_HSM_H
