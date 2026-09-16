#ifndef PULSE_HSM_H
#define PULSE_HSM_H

#include <Arduino.h>
#include <stdint.h>
#include <stddef.h>

// ============================================================================
// Compile-time configuration
// ============================================================================
#ifndef PULSEHSM_MAX_EVENTS
#define PULSEHSM_MAX_EVENTS 8      // MUST be a power of two
#endif
#ifndef PULSEHSM_MAX_DEPTH
#define PULSEHSM_MAX_DEPTH 4       // max number of ANCESTORS a leaf may have
#endif

// When 0 (default), a transition to the state you are already in is a no-op:
// no exit/entry callbacks re-run. Set to 1 to force a full exit/re-entry.
#ifndef PULSEHSM_SELF_TRANSITION_FULL_REINIT
#define PULSEHSM_SELF_TRANSITION_FULL_REINIT 0
#endif

// Set to 0 to compile out every state-name string literal. On AVR the literals
// themselves live in SRAM (see the note on names below), so this is the switch
// that actually reclaims those bytes on a 2 KB part.
#ifndef PULSEHSM_NAMES
#define PULSEHSM_NAMES 1
#endif

static_assert(PULSEHSM_MAX_EVENTS > 0 &&
              (PULSEHSM_MAX_EVENTS & (PULSEHSM_MAX_EVENTS - 1)) == 0,
              "PULSEHSM_MAX_EVENTS must be a power of two.");
static_assert(PULSEHSM_MAX_DEPTH >= 1,
              "PULSEHSM_MAX_DEPTH must be >= 1.");

// ============================================================================
// Where the state table actually lives
// ============================================================================
// `constexpr` guarantees compile-time INITIALISATION. It does not guarantee
// flash RESIDENCY, and the two are routinely confused.
//
//   * Flash-mapped targets (ESP32, Cortex-M, RP2040): a const/constexpr global
//     is placed in .rodata, which is memory-mapped and directly addressable.
//     The table genuinely costs zero RAM, and a plain `_states[i].parent` read
//     is a normal load.
//
//   * AVR (Harvard architecture): .rodata has no address space of its own, so
//     the linker copies every const global into SRAM at startup. A constexpr
//     table on an Uno costs full SRAM (85 bytes for the 5-state example).
//     The only way to keep it in flash is the PROGMEM attribute plus
//     pgm_read_* accessors -- which is what PULSEHSM_TABLE and the private
//     field readers below provide.
//
// Mark your table with PULSEHSM_TABLE and the same source is zero-RAM on both.
#if defined(__AVR__)
  #include <avr/pgmspace.h>
  #define PULSEHSM_TABLE PROGMEM
  #define PULSEHSM_RD_I8(p)     ((int8_t)pgm_read_byte(p))
  #define PULSEHSM_RD_U32(p)    ((unsigned long)pgm_read_dword(p))
  #define PULSEHSM_RD_PTR(T, p) (reinterpret_cast<T>(pgm_read_ptr(p)))
#else
  #define PULSEHSM_TABLE
  #define PULSEHSM_RD_I8(p)     (*(p))
  #define PULSEHSM_RD_U32(p)    (*(p))
  #define PULSEHSM_RD_PTR(T, p) (*(p))
#endif

// Wrap state names with this so PULSEHSM_NAMES=0 removes the literals entirely.
// NOTE: the table's `name` POINTERS live in flash, but on AVR the string data
// they point at is ordinary .rodata and therefore still lands in SRAM.
#if PULSEHSM_NAMES
  #define PULSEHSM_NAME(s) (s)
#else
  #define PULSEHSM_NAME(s) (nullptr)
#endif

// ============================================================================
// Interrupt-safe critical section (RAII) - REMAINS ENTIRELY UNCHANGED
// ============================================================================
class PulseHSMCritical {
#if defined(__AVR__)
    uint8_t s_;
  public:
    PulseHSMCritical() : s_(SREG) { cli(); }
    ~PulseHSMCritical() { SREG = s_; }
#elif defined(ARDUINO_ARCH_RP2040)
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

// ============================================================================
// Static Compile-Time Schema Definitions
// ============================================================================
class PulseHSM {
public:
    using Action  = void (*)();
    using EventCb = bool (*)(uint8_t event);

    // This data structure describes your state properties completely.
    // Field order is chosen so the struct packs without padding on AVR.
    struct StaticState {
        const char* name;
        Action update;
        Action entry;
        Action exit;
        unsigned long timeoutMs;
        int8_t timeoutNext;
        EventCb onEvent;
        int8_t parent;
        int8_t initialChild;
    };

    // Takes a pointer to the read-only table plus its element count.
    PulseHSM(const StaticState* stateTable, uint8_t count);

    // Start the machine. Returns false if startState is out of range or does
    // not resolve to a leaf (both are caught at compile time by
    // PULSEHSM_VALIDATE_TABLE, so this is a belt-and-braces runtime check).
    bool begin(int startState);

    // Run the scheduler. Call once per loop().
    void update();

    // Request a transition. Applied by update(), never mid-callback.
    void transitionTo(int newState);

    // Enqueue an event (interrupt safe). Returns false if the queue is full.
    bool sendEvent(uint8_t event, int32_t data = 0);

    // Getters - interface remains consistent with existing applications.
    int getCurrentState() const;
    const char* getStateName(int idx) const;
    const char* getCurrentName() const;
    unsigned long getStateElapsed() const;
    bool isInHierarchy(int state) const;

    int32_t getEventData() const;
    int getPreviousState() const;
    const char* getPreviousName() const;

    // Diagnostics: how many sendEvent() calls were rejected by a full queue.
    // Cheap to keep, and the only way to see an overflow that happened in an
    // ISR. Saturates at 255.
    uint8_t getDroppedEvents() const { return _dropped; }

private:
    const StaticState* _states;   // -> flash on every supported architecture
    uint8_t _stateCount;

    int8_t  currentState;
    int8_t  previousState;
    int8_t  pendingState;
    unsigned long entryTime;

    uint8_t evtQueue[PULSEHSM_MAX_EVENTS];
    int32_t evtData[PULSEHSM_MAX_EVENTS];
    uint8_t evtHead;
    uint8_t evtCount;
    uint8_t _dropped;
    int32_t currentEventData;
    bool inTransition;

    // --- hot-path cache -----------------------------------------------------
    // Rebuilt once per transition instead of re-walked every loop(). Without
    // this, each update() re-reads the timeout fields and re-walks the parent
    // chain, which on AVR means a pgm_read per link per iteration.
    unsigned long _curTimeoutMs;
    int8_t  _curTimeoutNext;
    Action  _updateChain[PULSEHSM_MAX_DEPTH + 1];  // root -> leaf, non-null only
    uint8_t _updateCount;

    // --- table field readers (compile to a plain load off AVR) --------------
    const char* _nameOf(int i)         const { return PULSEHSM_RD_PTR(const char*, &_states[i].name); }
    Action      _updateOf(int i)       const { return PULSEHSM_RD_PTR(Action,  &_states[i].update); }
    Action      _entryOf(int i)        const { return PULSEHSM_RD_PTR(Action,  &_states[i].entry); }
    Action      _exitOf(int i)         const { return PULSEHSM_RD_PTR(Action,  &_states[i].exit); }
    EventCb     _onEventOf(int i)      const { return PULSEHSM_RD_PTR(EventCb, &_states[i].onEvent); }
    unsigned long _timeoutMsOf(int i)  const { return PULSEHSM_RD_U32(&_states[i].timeoutMs); }
    int8_t      _timeoutNextOf(int i)  const { return PULSEHSM_RD_I8(&_states[i].timeoutNext); }
    int8_t      _parentOf(int i)       const { return PULSEHSM_RD_I8(&_states[i].parent); }
    int8_t      _initialChildOf(int i) const { return PULSEHSM_RD_I8(&_states[i].initialChild); }

    void _callEntryChain(int state, int stopAt = -1);
    void _callExitChain(int state, int stopAt = -1);
    void _dispatchEvent(uint8_t evt);
    void _runUpdates();
    void _executeTransition(int toState);
    void _cacheCurrent();
    int  _findLCA(int a, int b) const;
    bool _isLeaf(int state) const;
    int  _resolveEntry(int s) const;
};

// ============================================================================
// Compile-time table validation
// ============================================================================
// Written as C++11-style single-return recursion so it works under the
// -std=gnu++11 that the Arduino AVR core still uses.
namespace PulseHSMCheck {

using S = PulseHSM::StaticState;

constexpr bool idxOk(int8_t v, int count) {
    return v == -1 || (v >= 0 && v < count);
}

// Every parent is -1 or a valid index, and never the state itself.
constexpr bool parentsOk(const S* t, int count, int i) {
    return i >= count
        ? true
        : (idxOk(t[i].parent, count) && t[i].parent != (int8_t)i) &&
          parentsOk(t, count, i + 1);
}

// Number of ancestors; returns a deliberately huge value on a cycle.
constexpr int ancestors(const S* t, int i, int guard) {
    return t[i].parent == -1 ? 0
         : guard > PULSEHSM_MAX_DEPTH ? 999
         : 1 + ancestors(t, t[i].parent, guard + 1);
}

// No parent chain exceeds PULSEHSM_MAX_DEPTH (also catches cycles).
constexpr bool depthOk(const S* t, int count, int i) {
    return i >= count
        ? true
        : (ancestors(t, i, 0) <= PULSEHSM_MAX_DEPTH) && depthOk(t, count, i + 1);
}

constexpr bool hasChild(const S* t, int count, int s, int i) {
    return i >= count ? false
         : t[i].parent == (int8_t)s ? true
         : hasChild(t, count, s, i + 1);
}

// A composite state must name an initialChild, and that child must really be
// its child. A leaf must not name one. This is what makes every state a legal
// transition target: _resolveEntry() is then guaranteed to land on a leaf.
constexpr bool initialOk(const S* t, int count, int i) {
    return i >= count
        ? true
        : ( t[i].initialChild == -1
              ? !hasChild(t, count, i, 0)
              : (idxOk(t[i].initialChild, count) &&
                 t[t[i].initialChild].parent == (int8_t)i) )
          && initialOk(t, count, i + 1);
}

// A timeout needs both a duration and a destination, or it can never fire.
constexpr bool timeoutOk(const S* t, int count, int i) {
    return i >= count
        ? true
        : (idxOk(t[i].timeoutNext, count) &&
           ((t[i].timeoutMs > 0) == (t[i].timeoutNext != -1)))
          && timeoutOk(t, count, i + 1);
}

} // namespace PulseHSMCheck

// Drop this under your table. Every structural mistake that used to be a
// silent runtime hang is now a named compiler error on your desktop.
#define PULSEHSM_VALIDATE_TABLE(table, count)                                      \
    static_assert(sizeof(table) / sizeof((table)[0]) == (size_t)(count),           \
        "PulseHSM: state count does not match the number of rows in the table.");  \
    static_assert(PulseHSMCheck::parentsOk((table), (count), 0),                   \
        "PulseHSM: a state has an out-of-range or self-referencing parent.");      \
    static_assert(PulseHSMCheck::depthOk((table), (count), 0),                     \
        "PulseHSM: parent chain exceeds PULSEHSM_MAX_DEPTH, or contains a cycle."); \
    static_assert(PulseHSMCheck::initialOk((table), (count), 0),                   \
        "PulseHSM: a composite state has no initialChild, a leaf has one, or the " \
        "initialChild is not actually a child of that state.");                    \
    static_assert(PulseHSMCheck::timeoutOk((table), (count), 0),                   \
        "PulseHSM: timeoutMs and timeoutNext must be set together.")

#endif // PULSE_HSM_H
