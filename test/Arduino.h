// Minimal Arduino shim so the PulseHSM logic can be compiled and unit-tested
// on a host machine (and in CI) without any board toolchain. Device builds use
// the real <Arduino.h>; this file is only on the include path for the test.
#ifndef PULSEHSM_TEST_ARDUINO_SHIM
#define PULSEHSM_TEST_ARDUINO_SHIM

#include <cstdint>

// Controllable millisecond clock for deterministic tests.
extern unsigned long __pulsehsm_test_clock;
inline unsigned long millis() { return __pulsehsm_test_clock; }
inline void delay(unsigned long ms) { __pulsehsm_test_clock += ms; }

// On the host there are no interrupts; the critical-section guard falls back to
// these no-ops (none of __AVR__ / __CORTEX_M / ARDUINO_ARCH_* are defined here).
inline void noInterrupts() {}
inline void interrupts() {}

#endif
