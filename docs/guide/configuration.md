# Configuration

All configuration is done with `#define` before `#include "PulseHSM.h"`, or via
`-D` compiler flags. Invalid values fail at **compile time** with a clear message.

## Macros

### `PULSEHSM_MAX_STATES`

**Default:** `8` | **Range:** 1–127

The maximum number of states a single `PulseHSM` instance can hold. State indices
are stored as `int8_t`, so the upper bound is 127.

```cpp
#define PULSEHSM_MAX_STATES 32
#include "PulseHSM.h"
```

Memory cost: each state is a `State` struct (~30–40 bytes on 32-bit platforms,
less on AVR). For 32 states on a 32-bit MCU, expect roughly 1 KB.

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
→ parent → grandparent → great-grandparent → root (4 ancestors). `addState()`
returns `-1` if adding a state would exceed this depth.

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

## Per-sketch overrides

Because the macros are `#ifndef`-guarded in `PulseHSM.h`, defining them before
the include wins:

```cpp
// At the top of your .ino or in a config header:
#define PULSEHSM_MAX_STATES  24
#define PULSEHSM_MAX_EVENTS  16
#define PULSEHSM_MAX_DEPTH    5
#include "PulseHSM.h"
```

## Multiple machines

Each `PulseHSM` instance is independent. If you have two machines in the same
sketch and they need different capacities, you currently cannot give them different
`MAX_STATES` values (the macros are global). Size for the larger machine. They do
not share any state.

```cpp
PulseHSM motorFsm;    // uses shared MAX_STATES / MAX_EVENTS
PulseHSM networkFsm;
```
