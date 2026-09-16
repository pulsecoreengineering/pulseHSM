#include "PulseHSM.h"

PulseHSM::PulseHSM(const StaticState* stateTable, uint8_t count) {
    _states = stateTable;
    _stateCount = count;

    currentState = 0;
    previousState = -1;
    pendingState = -1;
    entryTime = 0;
    evtHead = 0;
    evtCount = 0;
    _dropped = 0;
    currentEventData = 0;
    inTransition = false;

    _curTimeoutMs = 0;
    _curTimeoutNext = -1;
    _updateCount = 0;
}

// addState and setInitial are gone: structure is fixed at compile time and
// checked by PULSEHSM_VALIDATE_TABLE on your desktop, not on the MCU.

bool PulseHSM::_isLeaf(int state) const {
    for (int i = 0; i < _stateCount; i++)
        if (_parentOf(i) == state) return false;
    return true;
}

int PulseHSM::_resolveEntry(int s) const {
    int guard = 0;
    while (s >= 0 && _initialChildOf(s) != -1 && guard++ <= PULSEHSM_MAX_DEPTH)
        s = _initialChildOf(s);
    return s;
}

// Rebuild everything update() needs, once per transition.
void PulseHSM::_cacheCurrent() {
    int8_t path[PULSEHSM_MAX_DEPTH + 1];
    int depth = 0;
    int s = currentState;
    while (s != -1 && depth <= PULSEHSM_MAX_DEPTH) {
        path[depth++] = (int8_t)s;
        s = _parentOf(s);
    }
    _updateCount = 0;
    for (int i = depth - 1; i >= 0; i--) {         // outermost parent first
        Action u = _updateOf(path[i]);
        if (u) _updateChain[_updateCount++] = u;
    }
    _curTimeoutMs   = _timeoutMsOf(currentState);
    _curTimeoutNext = _timeoutNextOf(currentState);
}

bool PulseHSM::begin(int startState) {
    if (startState < 0 || startState >= _stateCount) return false;
    int leaf = _resolveEntry(startState);
    if (!_isLeaf(leaf)) return false;   // composite with no initial substate
    pendingState = -1;
    previousState = -1;
    currentState = (int8_t)leaf;
    entryTime = millis();
    evtHead = 0;
    evtCount = 0;
    inTransition = false;
    _cacheCurrent();
    _callEntryChain(leaf);
    return true;
}

void PulseHSM::update() {
    unsigned long now = millis();

    // 1. Drain the queue ONE event at a time, applying any transition that
    //    event requested before the next one is dispatched. Draining the whole
    //    queue first would dispatch every event against the pre-transition
    //    state and collapse several requested transitions into the last one.
    //    The loop is bounded so an ISR that floods the queue cannot stall
    //    loop() indefinitely; the remainder is handled next pass.
    for (uint8_t guard = 0; guard < PULSEHSM_MAX_EVENTS; guard++) {
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
        if (pendingState != -1) {
            _executeTransition(pendingState);
            now = millis();
        }
    }

    // 2. Automatic timeout, read from the per-transition cache.
    if (pendingState == -1 && _curTimeoutMs > 0 &&
        (now - entryTime) >= _curTimeoutMs && _curTimeoutNext != -1) {
        pendingState = _curTimeoutNext;
    }

    // 3. update() callbacks, outermost parent first, from the cached chain.
    _runUpdates();

    // 4. Apply a transition requested by a timeout or an update() callback.
    if (pendingState != -1) _executeTransition(pendingState);
}

void PulseHSM::transitionTo(int newState) {
    if (newState >= 0 && newState < _stateCount) pendingState = (int8_t)newState;
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
    if (_dropped != 255) _dropped++;
    return false;
}

int PulseHSM::getCurrentState() const { return currentState; }

const char* PulseHSM::getStateName(int idx) const {
    if (idx < 0 || idx >= _stateCount) return "";
    const char* n = _nameOf(idx);
    return n ? n : "";
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
        cur = _parentOf(cur);
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
        s = _parentOf(s);
    }
    for (int i = depth - 1; i >= 0; i--) {
        Action e = _entryOf(path[i]);
        if (e) e();
    }
}

void PulseHSM::_callExitChain(int state, int stopAt) {
    int s = state;
    while (s != -1 && s != stopAt) {
        Action x = _exitOf(s);
        if (x) x();
        s = _parentOf(s);
    }
}

void PulseHSM::_dispatchEvent(uint8_t evt) {
    int s = currentState;
    while (s != -1) {
        EventCb cb = _onEventOf(s);
        if (cb && cb(evt)) return;      // consumed, stop bubbling
        s = _parentOf(s);
    }
}

void PulseHSM::_runUpdates() {
    for (uint8_t i = 0; i < _updateCount; i++) _updateChain[i]();
}

void PulseHSM::_executeTransition(int toState) {
    if (inTransition) return;           // ignore re-entry from an entry/exit callback
    inTransition = true;
    int target = _resolveEntry(toState);
    int from = currentState;
    int lca = _findLCA(from, target);
    _callExitChain(from, lca);
    previousState = currentState;
    currentState = (int8_t)target;
    pendingState = -1;
    entryTime = millis();
    _cacheCurrent();
    _callEntryChain(target, lca);
    inTransition = false;
}

int PulseHSM::_findLCA(int a, int b) const {
#if PULSEHSM_SELF_TRANSITION_FULL_REINIT
    if (a == b) return _parentOf(a);
#endif
    int8_t ancestorsA[PULSEHSM_MAX_DEPTH + 1];
    int depthA = 0;
    int s = a;
    while (s != -1 && depthA <= PULSEHSM_MAX_DEPTH) {
        ancestorsA[depthA++] = (int8_t)s;
        s = _parentOf(s);
    }
    s = b;
    while (s != -1) {
        for (int i = 0; i < depthA; i++)
            if (ancestorsA[i] == s) return s;
        s = _parentOf(s);
    }
    return -1;
}
