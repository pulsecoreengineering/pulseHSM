# Configuration

All configuration is done with `#define` before `#include "PulseHSM.h"`, or via
`-D` compiler flags. Invalid values fail at **compile time** with a clear message.

## Macros

### `PULSEHSM_MAX_EVENTS`

**Default:** `8` | **Constraint:** must be a power of two

The ring buffer depth. The queue takes `MAX_EVENTS × 5` bytes
(`uint8_t` event + `int32_t` data per slot).

> A non-power-of-two value is a **compile-time error** — the ring buffer uses a
> bitmask for wrap-around, which only works correctly for powers of two.

```cpp
#define PULSEHSM_MAX_EVENTS 16
```

### `PULSEHSM_MAX_DEPTH`

**Default:** `4` | **Minimum:** 1

The maximum number of **ancestors** a leaf may have. A leaf at depth 4 means: leaf
→ parent → grandparent → great-grandparent → root (4 ancestors).
`PULSEHSM_VALIDATE_TABLE` raises a compiler error if any state in the table would
exceed this depth.

The depth also sizes stack arrays used internally during entry/exit chain traversal
(`int8_t path[PULSEHSM_MAX_DEPTH + 1]`), so higher values use a bit more stack.

```cpp
#define PULSEHSM_MAX_DEPTH 6   // allow up to 6 levels of nesting
```

### `PULSEHSM_SELF_TRANSITION_FULL_REINIT`

**Default:** `0`

Controls what happens when `transitionTo(currentState)` is called while already in
that state.

| Value | Behaviour |
|---|---|
| `0` | **Lightweight** (default): only `entryTime` is reset. `entry()` and `exit()` do **not** run. |
| `1` | **Full reinit**: `exit()` then `entry()` run for **that state only**. Ancestor states are untouched. |

```cpp
#define PULSEHSM_SELF_TRANSITION_FULL_REINIT 1
```

The `ConnectionRetry` example demonstrates both modes.

### `PULSEHSM_NAMES`

**Default:** `1`

Set to `0` to strip every state-name string from the binary. Useful on very
constrained AVR parts (2 KB SRAM). When `PULSEHSM_NAMES=0`, `getCurrentName()`,
`getStateName()`, and `getPreviousName()` return `""`.

```cpp
#define PULSEHSM_NAMES 0   // strip all name strings
```

### `PULSEHSM_TABLE`

Placement macro applied to `constexpr StaticState` array declarations. Expands to
`PROGMEM` on AVR (keeps the table in flash), empty on everything else. Always apply
it to your table definition:

```cpp
constexpr PulseHSM::StaticState TABLE[ST_COUNT] PULSEHSM_TABLE = { ... };
```

## State count

There is no `PULSEHSM_MAX_STATES` macro. The number of states is the element count
of your table (`ST_COUNT` in the enum). State indices are stored as `int8_t`, so the
maximum is 127 states per instance.

## Per-sketch overrides

Because the macros are `#ifndef`-guarded in `PulseHSM.h`, defining them before
the include wins:

```cpp
// At the top of your .ino or in a config header:
#define PULSEHSM_MAX_EVENTS  16
#define PULSEHSM_MAX_DEPTH    5
#include "PulseHSM.h"
```

## Multiple machines

Each `PulseHSM` instance is fully independent — different tables, different state
counts, and separate event queues. `PULSEHSM_MAX_EVENTS` and `PULSEHSM_MAX_DEPTH`
are global and apply to every instance.

```cpp
PulseHSM motorFsm(MOTOR_TABLE, MOTOR_COUNT);
PulseHSM networkFsm(NET_TABLE, NET_COUNT);
```
