# Platforms

PulseHSM compiles and is tested in CI on all of the following targets.

## AVR (Arduino Uno, Nano, Mega and similar)

The `PulseHSMCritical` guard saves and restores `SREG` (`cli()`, then restore), which is safe to nest. It works correctly when `sendEvent()` is called from a timer ISR while the main loop is inside `update()`.

- Timeouts use `millis()`, as on any Arduino.
- The core defines `LED_BUILTIN`.
- No CMSIS or platform-specific headers are needed.

## ESP32

The guard uses `noInterrupts()` and `interrupts()` (the fallback path), which map to `portDISABLE_INTERRUPTS()` on FreeRTOS. It is safe for ISRs on the same core.

For producers on the other core of the ESP32's two, wrap `sendEvent()` in a `portMUX_TYPE` spinlock, or use a FreeRTOS queue to deliver events to the core that runs `update()`.

Mark ISR callbacks with `IRAM_ATTR`:

```cpp
void IRAM_ATTR buttonISR() { fsm.sendEvent(EVT_PRESS); }
```

## RP2040 (Arduino-Pico / earlephilhower core)

The guard uses `save_and_disable_interrupts()` and `restore_interrupts()` from the Pico SDK, which the arduino-pico core always provides. It is safe to nest.

The cross-core caveat is the same as on ESP32. ISRs on the same core are safe, and producers on the other core need a `spin_lock` or a hardware FIFO.

```cpp
// Mark ISR with the right attribute for arduino-pico:
void __isr buttonISR() { fsm.sendEvent(EVT_PRESS); }
```

## SAMD (Arduino MKR Zero, Zero, M0 and similar)

The guard uses `__get_PRIMASK()`, `__disable_irq()` and `__enable_irq()` from the ARM CMSIS headers, which the arduino:samd core includes.

`LED_BUILTIN` is defined and `millis()` works normally.

## STM32 (STM32duino core)

This uses the same CMSIS guard as SAMD, and the stm32duino core provides the CMSIS headers.

With STM32CubeIDE or another non-Arduino toolchain, include the CMSIS headers yourself and provide a `millis()` shim. Alternatively, replace the `millis()` calls in `PulseHSM.cpp` with `HAL_GetTick()`.

## Teensy (TEENSYDUINO)

Detected through the `TEENSYDUINO` macro. It takes the CMSIS guard path, since Teensy cores include the ARM CMSIS headers.

## Other Arduino-compatible targets

Any target the guards above don't match falls through to:

```cpp
PulseHSMCritical() { noInterrupts(); }
~PulseHSMCritical() { interrupts(); }
```

This is not safe to nest, but it works for most single-core bare-metal targets where `sendEvent()` is only called from one ISR at a time.

## Host and desktop (test builds)

The test suite in `test/` builds natively with `g++`, so you don't need a board. A `test/Arduino.h` shim supplies `millis()` (backed by a controllable test clock), `noInterrupts()`, `interrupts()` and `cli()` stubs, so the library compiles without any board-specific headers.

```bash
bash test/run_tests.sh
```
