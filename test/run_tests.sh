#!/usr/bin/env bash
# Build and run the PulseHSM v2 host tests in both self-transition modes.
# Returns non-zero if any check fails (used by CI).
set -euo pipefail
cd "$(dirname "$0")"

# gnu++11 enables designated array initialisers ([N] = {...}), which avr-g++
# and the ESP32 toolchain also accept; strict -std=c++11 would reject them.
CXXFLAGS="-std=gnu++11 -Wall -Wextra -Werror -I. -I.."

echo "== Mode 0 (lightweight self-transition, default) =="
g++ $CXXFLAGS test_pulsehsm.cpp ../PulseHSM.cpp -o /tmp/pulsehsm_t0
/tmp/pulsehsm_t0

echo
echo "== Mode 1 (full-reinit self-transition) =="
g++ $CXXFLAGS -DPULSEHSM_SELF_TRANSITION_FULL_REINIT=1 \
    test_pulsehsm.cpp ../PulseHSM.cpp -o /tmp/pulsehsm_t1
/tmp/pulsehsm_t1

echo
echo "== AppStateMachine integration test =="
g++ $CXXFLAGS test_appstate.cpp ../PulseHSM.cpp -o /tmp/pulsehsm_tapp
/tmp/pulsehsm_tapp

echo
echo "All test binaries passed."
