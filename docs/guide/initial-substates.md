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

## setInitial()

```cpp
bool setInitial(int parent, int child);
```

Marks `child` as the default substate of `parent`. `child` must be a **direct**
child of `parent`. Returns `false` on bad indices or if `child` is not a direct
child.

```cpp
int ST_OP   = fsm.addState("OPERATIONAL", ...parent = -1...);
int ST_IDLE = fsm.addState("IDLE",        ...parent = ST_OP...);
// ...
fsm.setInitial(ST_OP, ST_IDLE);

fsm.begin(ST_OP);               // resolves to ST_IDLE
fsm.transitionTo(ST_OP);        // resolves to ST_IDLE
```

## Recursive resolution

Resolution is recursive. If the initial child is itself a composite with its own
initial child, PulseHSM walks down until it reaches a leaf.

```
ROOT  (initialChild = MID)
└── MID  (initialChild = LEAF)
    └── LEAF
```

```cpp
int ROOT = fsm.addState("ROOT", ...);
int MID  = fsm.addState("MID",  ..., ROOT);
int LEAF = fsm.addState("LEAF", ..., MID);
int OTHER = fsm.addState("OTHER", ...);   // sibling for navigation

fsm.setInitial(ROOT, MID);
fsm.setInitial(MID,  LEAF);

fsm.begin(OTHER);
fsm.update();
fsm.transitionTo(ROOT);   // resolves all the way down to LEAF
fsm.update();
// getCurrentState() == LEAF
// entry order: ROOT entry(), MID entry(), LEAF entry()
```

## Entry order

Entry callbacks fire **outer-to-inner**: the composite's `entry()` runs first,
then the initial child's, all the way to the resolved leaf. Exit callbacks fire
**inner-to-outer**.

```
transitionTo(ST_OP) while in ST_FAULT:
  1. FAULT exit()
  2. ST_OP entry()
  3. ST_IDLE entry()      ← resolved initial substate
```

## begin() with a composite

`begin()` accepts a composite state as long as it has an initial substate
configured (recursively). A composite with **no** `setInitial` is rejected (returns
`false`) — this is a programming error, not a runtime condition.

```cpp
fsm.begin(ST_OP);   // ✓ ST_OP.initialChild = ST_IDLE (a leaf)
fsm.begin(ST_ROOT); // ✓ ST_ROOT → ST_MID → ST_LEAF (recursive)
fsm.begin(ST_OP);   // ✗ if setInitial was never called on ST_OP → returns false
```

## Backward compatibility

Code that never calls `setInitial()` is unaffected. `initialChild` defaults to
`-1`, so `_resolveEntry()` returns its input unchanged — every existing transition
and `begin()` call behaves identically.

## Common mistakes

**Wrong: child is not a direct child**

```cpp
// ROOT → MID → LEAF
fsm.setInitial(ROOT, LEAF);   // ✗ returns false — LEAF is a grandchild
fsm.setInitial(ROOT, MID);    // ✓
```

**Wrong: forgetting to add parent before child**

`addState()` requires the parent to be added first (its index is what you pass as
`parent`). If you add a child before its parent, the parent index is unknown.

```cpp
// Correct order:
int PARENT = fsm.addState("PARENT", ..., -1);
int CHILD  = fsm.addState("CHILD",  ..., PARENT);  // PARENT already has an index
```
