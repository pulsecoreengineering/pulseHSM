#!/usr/bin/env bash
# Build and run the PulseHSM host tests in both self-transition modes.
# Returns non-zero if any check fails (used by CI).
set -euo pipefail
cd "$(dirname "$0")"

CXXFLAGS="-std=c++11 -Wall -Wextra -Werror -I. -I.."

echo "== Mode 0 (lightweight self-transition, default) =="
g++ $CXXFLAGS test_pulsehsm.cpp ../PulseHSM.cpp -o /tmp/pulsehsm_t0
/tmp/pulsehsm_t0

echo
echo "== Mode 1 (full-reinit self-transition) =="
g++ $CXXFLAGS -DPULSEHSM_SELF_TRANSITION_FULL_REINIT=1 \
    test_pulsehsm.cpp ../PulseHSM.cpp -o /tmp/pulsehsm_t1
/tmp/pulsehsm_t1

echo
echo "All test binaries passed."
