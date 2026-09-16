# Initial Substates

An **initial substate** (UML: *initial pseudostate*) is the default child entered
when a composite state is targeted. Without it, you must always name the exact leaf
you want — with it, you can transition to a superstate and have the machine find its
own starting point.

## The problem it solves

Suppose you have this hierarchy:

```
OPERATIONAL
├── IDLE         ← you always want to land here when entering OPERATIONAL
├── PROCESSING
└── FINISHING
FAULT
```

Without initial substates, every transition that wants to "go back to the
operational area" must say `transitionTo(ST_IDLE)`. That's fine for two states, but
in a real machine `OPERATIONAL` might have a half-dozen leaves, and the "correct
entry point" may depend on the subgraph, not the caller.

## The `initialChild` field

Every `StaticState` has an `initialChild` field — the ninth and last positional
field in the struct. Set it to the index of a direct child to make that child the
default entry point; leave it as `-1` for leaf states or composites with no
designated initial child.

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

Resolution is recursive. If the initial child is itself a composite with its own
`initialChild`, PulseHSM walks down until it reaches a leaf.

```
ROOT  (initialChild = ST_MID)
└── MID  (initialChild = ST_LEAF)
    └── LEAF
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

Entry callbacks fire **outer-to-inner**: the composite's `entry()` runs first,
then the initial child's, all the way to the resolved leaf. Exit callbacks fire
**inner-to-outer**.

```
transitionTo(ST_OPERATIONAL) while in ST_FAULT:
  1. FAULT exit()
  2. OPERATIONAL entry()
  3. IDLE entry()      ← resolved initial substate
```

## begin() with a composite

`begin()` accepts a composite state as long as it has an `initialChild` configured
(recursively). A composite with `initialChild = -1` is rejected (returns `false`) —
this is a programming error caught at compile time by `PULSEHSM_VALIDATE_TABLE`.

```cpp
fsm.begin(ST_OPERATIONAL); // ✓  initialChild = ST_IDLE (a leaf)
fsm.begin(ST_ROOT);        // ✓  ST_ROOT → ST_MID → ST_LEAF (recursive)
```

## Compile-time validation

`PULSEHSM_VALIDATE_TABLE` raises a compiler error if a composite state's
`initialChild` does not point to a direct child, catching wiring mistakes before
any code runs on the MCU.

## Common mistakes

**Wrong: `initialChild` is not a direct child**

If `ROOT → MID → LEAF`, you cannot set `ROOT.initialChild = ST_LEAF` — LEAF is a
grandchild of ROOT. Set `ROOT.initialChild = ST_MID` and `MID.initialChild = ST_LEAF`.
`PULSEHSM_VALIDATE_TABLE` catches this at compile time.

**Wrong: composite with no `initialChild`**

If you call `fsm.begin(ST_COMPOSITE)` or `fsm.transitionTo(ST_COMPOSITE)` where
`ST_COMPOSITE.initialChild == -1`, `begin()` returns `false` and the transition is
ignored. `PULSEHSM_VALIDATE_TABLE` flags this at compile time so it never reaches
the MCU.
