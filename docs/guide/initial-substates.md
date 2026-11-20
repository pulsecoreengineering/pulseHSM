# Initial substates

An initial substate (UML calls it an initial pseudostate) is the default child entered when a composite state is targeted. Without one, you always have to name the exact leaf you want. With one, you can transition to a superstate and the machine finds its own starting point.

## The problem it solves

Take this hierarchy:

```
OPERATIONAL
  IDLE         (you always want to land here when entering OPERATIONAL)
  PROCESSING
  FINISHING
FAULT
```

Without initial substates, every transition that means "go back to the operational area" has to say `transitionTo(ST_IDLE)`. That's fine with two states. In a real machine `OPERATIONAL` might have half a dozen leaves, and the right entry point may depend on the subgraph, not on the caller.

## The `initialChild` field

Every `StaticState` has an `initialChild` field, the ninth and last positional field in the struct. Set it to the index of a direct child to make that child the default entry point. Leave it as `-1` for leaf states, or for composites with no designated initial child.

```cpp
enum StateID : int8_t {
    ST_OPERATIONAL = 0, ST_IDLE, ST_PROCESSING, ST_FINISHING,
    ST_FAULT, ST_COUNT
};

constexpr PulseHSM::StaticState TABLE[ST_COUNT] PULSEHSM_TABLE = {
    //                          name           upd   entry  exit  ms  next  event  parent  initialChild
    [ST_OPERATIONAL] = { PULSEHSM_NAME("OPERATIONAL"), nullptr, nullptr, nullptr, 0, -1, nullptr, -1, ST_IDLE },
    [ST_IDLE]        = { PULSEHSM_NAME("IDLE"),        nullptr, idleEntry, nullptr, 0, -1, idleEvent, ST_OPERATIONAL, -1 },
    // ... other children
    [ST_FAULT]       = { PULSEHSM_NAME("FAULT"),       nullptr, faultEntry, nullptr, 0, -1, nullptr, -1, -1 },
};

fsm.begin(ST_OPERATIONAL);       // resolves to ST_IDLE
fsm.transitionTo(ST_OPERATIONAL); // resolves to ST_IDLE
```

## Recursive resolution

Resolution is recursive. If the initial child is itself a composite with its own `initialChild`, PulseHSM keeps walking down until it reaches a leaf.

```
ROOT  (initialChild = ST_MID)
  MID  (initialChild = ST_LEAF)
    LEAF
```

```cpp
enum StateID : int8_t { ST_ROOT = 0, ST_MID, ST_LEAF, ST_OTHER, ST_COUNT };

constexpr PulseHSM::StaticState TABLE[ST_COUNT] PULSEHSM_TABLE = {
    [ST_ROOT]  = { PULSEHSM_NAME("ROOT"),  nullptr, nullptr,     nullptr, 0, -1, nullptr, -1,       ST_MID  },
    [ST_MID]   = { PULSEHSM_NAME("MID"),   nullptr, nullptr,     nullptr, 0, -1, nullptr, ST_ROOT,  ST_LEAF },
    [ST_LEAF]  = { PULSEHSM_NAME("LEAF"),  nullptr, leafEntry,   nullptr, 0, -1, nullptr, ST_MID,   -1      },
    [ST_OTHER] = { PULSEHSM_NAME("OTHER"), nullptr, otherEntry,  nullptr, 0, -1, nullptr, -1,       -1      },
};

fsm.begin(ST_OTHER);
fsm.update();
fsm.transitionTo(ST_ROOT);   // resolves all the way down to LEAF
fsm.update();
// getCurrentState() == ST_LEAF
// entry order: ROOT entry(), MID entry(), LEAF entry()
```

## Entry order

Entry callbacks fire from the outside in. The composite's `entry()` runs first, then the initial child's, and so on down to the resolved leaf. Exit callbacks fire from the inside out.

For `transitionTo(ST_OPERATIONAL)` while in `ST_FAULT`, the order is:

```
1. FAULT exit()
2. OPERATIONAL entry()
3. IDLE entry()      (the resolved initial substate)
```

## begin() with a composite

`begin()` accepts a composite state as long as it has an `initialChild` configured, recursively. A composite with `initialChild = -1` is rejected and `begin()` returns `false`. That is a programming error, and `PULSEHSM_VALIDATE_TABLE` catches it at compile time.

```cpp
fsm.begin(ST_OPERATIONAL); // ok: initialChild is ST_IDLE (a leaf)
fsm.begin(ST_ROOT);        // ok: ST_ROOT resolves to ST_MID, then to ST_LEAF
```

## Compile-time validation

`PULSEHSM_VALIDATE_TABLE` raises a compiler error if a composite state's `initialChild` doesn't point to a direct child. That catches wiring mistakes before any code runs on the MCU.

## Common mistakes

### `initialChild` is not a direct child

Say `ROOT` contains `MID`, and `MID` contains `LEAF`. You can't set `ROOT.initialChild = ST_LEAF`, because `LEAF` is a grandchild of `ROOT`. Set `ROOT.initialChild = ST_MID` and `MID.initialChild = ST_LEAF`. `PULSEHSM_VALIDATE_TABLE` catches this at compile time.

### A composite with no `initialChild`

Suppose you call `fsm.begin(ST_COMPOSITE)` or `fsm.transitionTo(ST_COMPOSITE)` and `ST_COMPOSITE.initialChild == -1`. Then `begin()` returns `false` and the transition is ignored. `PULSEHSM_VALIDATE_TABLE` flags this at compile time, so it never reaches the MCU.
