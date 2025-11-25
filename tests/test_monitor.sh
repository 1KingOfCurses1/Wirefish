# /*
#  * File: tests/test_monitor.c
#  * Summary: Unit tests for interface bandwidth monitoring using /proc/net/dev.
#  *
#  * Purpose:
#  *  - Validate parsing of /proc/net/dev for a given interface.
#  *  - Verify rate computations over time (bps) and rolling averages.
#  *  - Ensure robustness to missing/nonexistent interfaces.
#  *
#  * Test Matrix (examples):
#  *  - Happy path on loopback:
#  *      cfg.iface = "lo", interval_ms small (e.g., 100–200 ms), duration_sec short (1–2 s).
#  *      Expect non-decreasing byte counters; rates >= 0.
#  *  - Nonexistent interface:
#  *      cfg.iface = "nope0" → expect error return (no crash).
#  *  - Smoothing:
#  *      Enable ring buffer; verify avg tracks mean of recent samples.
#  *  - Low-traffic vs burst:
#  *      Optionally generate traffic (e.g., ping localhost) and confirm rate spike > baseline.
#  *
#  * What is Verified:
#  *  - Return codes and series length > 0 on success.
#  *  - Each sample has iface set, rx/tx bytes monotonic (except counter wrap edge case).
#  *  - Rates are finite, >= 0; averages within expected bounds.
#  *  - Proper resource cleanup (no leaks).
#  *
#  * Modules Covered:
#  *  - monitor.c / monitor.h
#  *  - ringbuf.c / ringbuf.h
#  *  - timeutil.c (interval timing)
#  *
#  * Fixtures / Setup:
#  *  - Read-only access to /proc/net/dev; skip test if file unavailable.
#  *  - Interface name overridable via env (NETGUARD_TEST_IFACE).
#  */

 #!/bin/bash
#
# A simple framework for testing the binconv (bctest) scripts
#
# Returns the number of failed test cases.
#
# Format of a test:
#     test 'command' expected_return_value 'stdin text' 'expected stdout' 'expected stderr'
#
# Some example test cases are given. You should add more test cases.
#
# Sam Scott, McMaster University, 2025


# Simple test script for scanner module
declare -i tc=0
declare -i fails=0

run_test() {
    tc=$tc+1

    local CMD="$1"
    local WANT_RET="$2"
    local WANT_OUT="$3"
    local WANT_ERR="$4"

    $CMD >tmp_out 2>tmp_err
    local GOT_RET=$?

    if [[ "$GOT_RET" != "$WANT_RET" ]]; then
        echo "Test $tc FAILED (return code)"
        fails=$fails+1
        return
    fi

    local OUT="$(cat tmp_out)"
    local ERR="$(cat tmp_err)"

    if [[ -n "$WANT_OUT" ]]; then
        if [[ "$OUT" != *"$WANT_OUT"* ]]; then
            echo "Test $tc FAILED (stdout)"
            echo "  Expected STDOUT $WANT_OUT"
            echo "  Actual STDOUT $OUT"
            fails=$fails+1
            return
        fi
    fi

    if [[ -n "$WANT_ERR" ]]; then
        if [[ "$ERR" != *"$WANT_ERR"* ]]; then
            echo "Test $tc FAILED (stderr)"
            echo "  Expected STDOUT: $WANT_ERR"
            echo "  Actual STDOUT: $ERR"
            fails=$fails+1
            return
        fi
    fi

    echo "Test $tc passed"
}


#######################################
#           test cases                #
#######################################

# 1 — missing interval
run_test "./wirefish --monitor" 0 ""

# 2 — invalid interval string
run_test "./wirefish --monitor --interval abc" 1 "" "Error: Invalid interval value"

# 3 — zero interval
run_test "./wirefish --monitor --interval 0" 1 "" "Error: Interval must be positive"

# 4 — negative interval
run_test "./wirefish --monitor --interval -50" 1 "" "Error: Interval must be positive"

# 5 — nonexistent interface
run_test "./wirefish --monitor --iface definitelyNotReal --interval 200" 0 ""

# 6 — auto-detect interface 
run_test "./wirefish --monitor --interval 200" 0 ""

# 7 — interface lo exists
run_test "./wirefish --monitor --iface lo --interval 200" 0 ""

# 8 — json + csv both true  
run_test "./wirefish --monitor --interval 200 --json --csv" 1 "" "Error: Cannot use both"

# 9 — unknown argument
run_test "./wirefish --monitor --interval 200 --what" 1 "" "Error: Unknown argument"

# 10 — monitor + scan together --- not allowed (cli.c)
run_test "./wirefish --monitor --scan --interval 200" 1 "" "Error: Only one mode"

# 11 — running with no mode at all
run_test "./wirefish" 1 "" "Error: Must specify one mode"

# 12 — using --ttl in monitor is allowed
run_test "./wirefish --monitor --ttl 1-5 --interval 200" 0 ""

# 13 — weird iface name 
run_test "./wirefish --monitor --iface ###weird### --interval 200" 0 ""

# 14 — user gives --csv alone
run_test "./wirefish --monitor --interval 200 --csv" 0 ""
