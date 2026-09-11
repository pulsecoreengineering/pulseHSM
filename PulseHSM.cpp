#include "PulseHSM.h"
#include <string.h>

PulseHSM::PulseHSM() {
    memset(states, 0, sizeof(states));
    stateCount = 0;
    currentState = 0;
    previousState = -1;
    pendingState = -1;
    entryTime = 0;
    evtHead = 0;
    evtCount = 0;
    currentEventData = 0;
    inTransition = false;
}

int PulseHSM::addState(const char* name, Action update, Action entry, Action exit,
                       unsigned long timeoutMs, int timeoutNext, EventCb onEvent, int parent) {
    if (stateCount >= PULSEHSM_MAX_STATES) return -1;

    // Reject hierarchies deeper than the configured bound instead of silently
    // truncating the entry/exit/update chains at run time.
    int d = 0;
    for (int p = parent; p != -1; p = states[p].parent) {
        if (++d > PULSEHSM_MAX_DEPTH) return -1;
    }

    State& s = states[stateCount];
    s.name = name;
    s.update = update;
    s.entry = entry;
    s.exit = exit;
    s.timeoutMs = timeoutMs;
    s.timeoutNext = timeoutNext;
    s.onEvent = onEvent;
    s.parent = (int8_t)parent;
    s.initialChild = -1;
    return stateCount++;
}

bool PulseHSM::_isLeaf(int state) const {
    for (int i = 0; i < stateCount; i++)
        if (states[i].parent == state) return false;
    return true;
}

bool PulseHSM::setInitial(int parent, int child) {
    if (parent < 0 || parent >= stateCount) return false;
    if (child < 0 || child >= stateCount) return false;
    if (states[child].parent != (int8_t)parent) return false;
    states[parent].initialChild = (int8_t)child;
    return true;
}

int PulseHSM::_resolveEntry(int s) const {
    int guard = 0;
    while (s >= 0 && states[s].initialChild != -1 && guard++ <= PULSEHSM_MAX_DEPTH)
        s = states[s].initialChild;
    return s;
}

bool PulseHSM::begin(int startState) {
    if (startState < 0 || startState >= stateCount) return false;
    int leaf = _resolveEntry(startState);
    if (!_isLeaf(leaf)) return false;   // composite with no initial substate set
    pendingState = -1;
    previousState = -1;
    currentState = leaf;
    entryTime = millis();
    evtHead = 0;
    evtCount = 0;
    inTransition = false;
    _callEntryChain(leaf);
    return true;
}

void PulseHSM::update() {
    // Dispatch queued events. Pop one at a time under a short critical section,
    // then run the (potentially slow) handler with interrupts restored.
    while (true) {
        uint8_t evt = 0;
        bool have = false;
        {
            PulseHSMCritical crit;
            if (evtCount > 0) {
                evt = evtQueue[evtHead];
                currentEventData = evtData[evtHead];
                evtHead = (uint8_t)((evtHead + 1) & (PULSEHSM_MAX_EVENTS - 1));
                evtCount--;
                have = true;
            }
        }
        if (!have) break;
        _dispatchEvent(evt);
    }

    // Auto timeout
    if (pendingState == -1 && states[currentState].timeoutMs > 0 &&
        (millis() - entryTime) >= states[currentState].timeoutMs &&
        states[currentState].timeoutNext != -1) {
        pendingState = states[currentState].timeoutNext;
    }
    _runUpdates();
    if (pendingState != -1) _executeTransition(pendingState);
}

void PulseHSM::transitionTo(int newState) {
    if (newState >= 0 && newState < stateCount) pendingState = newState;
}

bool PulseHSM::sendEvent(uint8_t event, int32_t data) {
    PulseHSMCritical crit;
    if (evtCount < PULSEHSM_MAX_EVENTS) {
        uint8_t slot = (uint8_t)((evtHead + evtCount) & (PULSEHSM_MAX_EVENTS - 1));
        evtQueue[slot] = event;
        evtData[slot] = data;
        evtCount++;
        return true;
    }
    return false;   // queue full — event dropped (bounded, no dynamic allocation)
}

int PulseHSM::getCurrentState() const { return currentState; }
const char* PulseHSM::getStateName(int idx) const {
    if (idx < 0 || idx >= stateCount) return "";
    return states[idx].name ? states[idx].name : "";
}
const char* PulseHSM::getCurrentName() const { return getStateName(currentState); }
unsigned long PulseHSM::getStateElapsed() const { return millis() - entryTime; }
int32_t PulseHSM::getEventData() const { return currentEventData; }
int PulseHSM::getPreviousState() const { return previousState; }
const char* PulseHSM::getPreviousName() const { return getStateName(previousState); }

bool PulseHSM::isInHierarchy(int s) const {
    int cur = currentState;
    while (cur != -1) {
        if (cur == s) return true;
        cur = states[cur].parent;
    }
    return false;
}

// Private methods ------------------------------------------------------------
void PulseHSM::_callEntryChain(int state, int stopAt) {
    int8_t path[PULSEHSM_MAX_DEPTH + 1];
    int depth = 0;
    int s = state;
    while (s != -1 && s != stopAt && depth <= PULSEHSM_MAX_DEPTH) {
        path[depth++] = (int8_t)s;
        s = states[s].parent;
    }
    for (int i = depth - 1; i >= 0; i--)
        if (states[path[i]].entry) states[path[i]].entry();
}

void PulseHSM::_callExitChain(int state, int stopAt) {
    int s = state;
    while (s != -1 && s != stopAt) {
        if (states[s].exit) states[s].exit();
        s = states[s].parent;
    }
}

void PulseHSM::_dispatchEvent(uint8_t evt) {
    int s = currentState;
    while (s != -1) {
        if (states[s].onEvent && states[s].onEvent(evt)) return;
        s = states[s].parent;
    }
    // unhandled (optional debug hook)
}

void PulseHSM::_runUpdates() {
    int8_t ancestors[PULSEHSM_MAX_DEPTH];
    int depth = 0;
    int s = states[currentState].parent;
    while (s != -1 && depth < PULSEHSM_MAX_DEPTH) {
        ancestors[depth++] = (int8_t)s;
        s = states[s].parent;
    }
    for (int i = depth - 1; i >= 0; i--)
        if (states[ancestors[i]].update) states[ancestors[i]].update();
    if (states[currentState].update) states[currentState].update();
}

void PulseHSM::_executeTransition(int toState) {
    if (inTransition) return;
    inTransition = true;
    int target = _resolveEntry(toState);
    int from = currentState;
    int lca = _findLCA(from, target);
    _callExitChain(from, lca);
    previousState = currentState;
    currentState = target;
    pendingState = -1;
    entryTime = millis();
    _callEntryChain(target, lca);
    inTransition = false;
}

int PulseHSM::_findLCA(int a, int b) const {
#if PULSEHSM_SELF_TRANSITION_FULL_REINIT
    // Full-reinit self-transition: exit and re-enter THIS state only. Stopping
    // both chains at the parent leaves every ancestor untouched (a UML external
    // self-transition on a leaf does not tear down its superstates).
    if (a == b) return states[a].parent;
#endif
    // Normal lowest-common-ancestor search. For a == b in lightweight mode this
    // returns `a`, so neither chain runs and only the timer resets.
    int8_t ancestorsA[PULSEHSM_MAX_DEPTH + 1];
    int depthA = 0;
    int s = a;
    while (s != -1 && depthA <= PULSEHSM_MAX_DEPTH) {
        ancestorsA[depthA++] = (int8_t)s;
        s = states[s].parent;
    }
    s = b;
    while (s != -1) {
        for (int i = 0; i < depthA; i++)
            if (ancestorsA[i] == s) return s;
        s = states[s].parent;
    }
    return -1;
}
