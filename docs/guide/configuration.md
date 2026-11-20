# Configuration

All configuration is done with `#define` before `#include "PulseHSM.h"`, or with `-D` compiler flags. Invalid values fail at compile time with a clear message.

## Macros

### `PULSEHSM_MAX_EVENTS`

Default: `8`. Must be a power of two.

The depth of the event ring buffer. Each slot holds a `uint8_t` event and an `int32_t` data value, 5 bytes in all, so the queue takes `PULSEHSM_MAX_EVENTS * 5` bytes.

A value that isn't a power of two is a compile-time error. The ring buffer wraps around with a bitmask, which only works for powers of two.

```cpp
#define PULSEHSM_MAX_EVENTS 16
```

### `PULSEHSM_MAX_DEPTH`

Default: `4`. Minimum: `1`.

The maximum number of ancestors a leaf may have. A leaf at depth 4 has four ancestors: parent, grandparent, great-grandparent and root. `PULSEHSM_VALIDATE_TABLE` gives a compiler error if any state in the table is deeper than this.

The depth also sizes a stack array used while walking the entry and exit chains (`int8_t path[PULSEHSM_MAX_DEPTH + 1]`), so higher values use a little more stack.

```cpp
#define PULSEHSM_MAX_DEPTH 6   // allow up to 6 levels of nesting
```

### `PULSEHSM_SELF_TRANSITION_FULL_REINIT`

Default: `0`.

Sets what happens when `transitionTo(currentState)` is called while the machine is already in that state.

| Value | Behaviour |
|---|---|
| `0` | Lightweight, the default. Only `entryTime` is reset. `entry()` and `exit()` don't run. |
| `1` | Full reinit. `exit()` then `entry()` run for that state only. Ancestor states are untouched. |

```cpp
#define PULSEHSM_SELF_TRANSITION_FULL_REINIT 1
```

The `ConnectionRetry` example shows both modes.

### `PULSEHSM_NAMES`

Default: `1`.

Set it to `0` to strip every state-name string from the binary. This helps on very constrained AVR parts (2 KB SRAM). With `PULSEHSM_NAMES=0`, `getCurrentName()`, `getStateName()` and `getPreviousName()` return `""`.

```cpp
#define PULSEHSM_NAMES 0   // strip all name strings
```

### `PULSEHSM_TABLE`

A placement macro for `constexpr StaticState` array declarations. On AVR it expands to `PROGMEM`, which keeps the table in flash. Everywhere else it is empty. Put it on every table definition:

```cpp
constexpr PulseHSM::StaticState TABLE[ST_COUNT] PULSEHSM_TABLE = { ... };
```

## State count

There is no `PULSEHSM_MAX_STATES` macro. The number of states is the element count of your table (`ST_COUNT` in the enum). State indices are stored as `int8_t`, so one instance can have at most 127 states.

## Per-sketch overrides

The macros are `#ifndef`-guarded in `PulseHSM.h`, so a definition placed before the include wins:

```cpp
// At the top of your .ino or in a config header:
#define PULSEHSM_MAX_EVENTS  16
#define PULSEHSM_MAX_DEPTH    5
#include "PulseHSM.h"
```

## Multiple machines

Each `PulseHSM` instance is independent, with its own table, state count and event queue. `PULSEHSM_MAX_EVENTS` and `PULSEHSM_MAX_DEPTH` are global and apply to every instance.

```cpp
PulseHSM motorFsm(MOTOR_TABLE, MOTOR_COUNT);
PulseHSM networkFsm(NET_TABLE, NET_COUNT);
```
