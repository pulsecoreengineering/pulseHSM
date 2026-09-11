# Platforms

PulseHSM compiles and is tested in CI on all of the following targets.

## AVR (Arduino Uno, Nano, Mega…)

The `PulseHSMCritical` guard uses `SREG` save/restore (`cli()` / restore), which
is nesting-safe. Works correctly when `sendEvent()` is called from a timer ISR
while the main loop is inside `update()`.

- `millis()` is used for timeouts — standard Arduino.
- `LED_BUILTIN` is defined by the core.
- No CMSIS or platform-specific headers required.

## ESP32

The guard uses `noInterrupts()` / `interrupts()` (the fallback path), which maps
to `portDISABLE_INTERRUPTS()` on FreeRTOS. Safe for ISRs on the **same core**.

> For cross-core producers on the ESP32's dual cores, wrap `sendEvent()` in a
> `portMUX_TYPE` spinlock or use a FreeRTOS queue to deliver to the update core.

Mark ISR callbacks with `IRAM_ATTR`:

```cpp
void IRAM_ATTR buttonISR() { fsm.sendEvent(EVT_PRESS); }
```

## RP2040 (Arduino-Pico / earlephilhower core)

The guard uses `save_and_disable_interrupts()` / `restore_interrupts()` from the
Pico SDK, which the arduino-pico core always provides. This is nesting-safe.

> Cross-core: same caveat as ESP32 — same-core ISRs are safe, cross-core producers
> need a `spin_lock` or a hardware FIFO.

```cpp
// Mark ISR with the right attribute for arduino-pico:
void __isr buttonISR() { fsm.sendEvent(EVT_PRESS); }
```

## SAMD (Arduino MKR Zero, Zero, M0…)

The guard uses `__get_PRIMASK()` / `__disable_irq()` / `__enable_irq()` from the
ARM CMSIS headers, which the arduino:samd core includes.

`LED_BUILTIN` is defined. `millis()` works normally.

## STM32 (STM32duino / stm32duino core)

Same CMSIS guard as SAMD. The stm32duino core provides CMSIS headers.

If you are using the STM32CubeIDE or a non-Arduino toolchain, include the CMSIS
headers manually and provide a `millis()` shim, or replace the `millis()` calls
in `PulseHSM.cpp` with `HAL_GetTick()`.

## Teensy (TEENSYDUINO)

Detected via the `TEENSYDUINO` macro. Uses the CMSIS guard path (Teensy cores
include the ARM CMSIS headers).

## Other / generic Arduino-compatible

Any target not matched by the guards above falls through to:

```cpp
PulseHSMCritical() { noInterrupts(); }
~PulseHSMCritical() { interrupts(); }
```

This is not nesting-safe but works for most single-core bare-metal targets where
`sendEvent()` is only called from one ISR at a time.

## Host / desktop (test builds)

The test suite in `test/` builds natively with `g++` — no board needed. A
`test/Arduino.h` shim provides `millis()` (backed by a controllable test clock),
`noInterrupts()`, `interrupts()`, and `cli()` stubs so the library compiles
without any board-specific headers.

```bash
bash test/run_tests.sh
```
