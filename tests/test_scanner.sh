#!/bin/bash

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
            echo "  expected substring: $WANT_OUT"
            echo "  got: $OUT"
            fails=$fails+1
            return
        fi
    fi

    if [[ -n "$WANT_ERR" ]]; then
        if [[ "$ERR" != *"$WANT_ERR"* ]]; then
            echo "Test $tc FAILED (stderr)"
            echo "  expected substring: $WANT_ERR"
            echo "  got: $ERR"
            fails=$fails+1
            return
        fi
    fi

    echo "Test $tc passed"
}

#######################################
#           test cases                #
#######################################

# 1 — Basic scan, single stable port (google.com:80)
run_test "./wirefish --scan --target google.com --ports 80-80" 0 "PORT" ""

# 2 — Multi-port google scan (80 open, others filtered)
run_test "./wirefish --scan --target google.com --ports 80-82" 0 "PORT" ""

# 3 — CSV header test
run_test "./wirefish --scan --target google.com --ports 80-82 --csv" 0 "port,state,latency_ms" ""

# 4 — JSON results test
run_test "./wirefish --scan --target google.com --ports 80-82 --json" 0 "\"results\"" ""

# 5 — Missing target (cli.c )
run_test "./wirefish --scan --ports 1-3" 1 "" "--target required for scan mode"

# 6 — Reversed port range (parse_range error)
run_test "./wirefish --scan --target google.com --ports 10-1" 1 "" "cannot be greater"

# 7 — Port < 1 (cli.c )
run_test "./wirefish --scan --target google.com --ports 0-5" 1 "" "Ports must be in range"

# 8 — Port > 65535 (cli.c)
run_test "./wirefish --scan --target google.com --ports 1-70000" 1 "" "Ports must be in range"

# 9 — Bad hostname
run_test "./wirefish --scan --target nohost123456 --ports 1-3" 1 "" "Failed to resolve target"

# 10 — Non-routable IP - always filtered
run_test "./wirefish --scan --target 10.255.255.1 --ports 80-81" 0 "filtered" ""

# 11 — Single highest valid port
run_test "./wirefish --scan --target google.com --ports 65535-65535" 0 "65535" ""

# 12 — Two modes specified (cli.c)
run_test "./wirefish --scan --trace --target google.com --ports 80-81" 1 "" "Only one mode"

# 13 — JSON + CSV not allowed together
run_test "./wirefish --scan --target google.com --ports 80-82 --json --csv" 1 "" "Cannot use both"

# 14 — No mode provided
run_test "./wirefish --target google.com --ports 80-82" 1 "" "Must specify one mode"

# 15 — Unknown argument
run_test "./wirefish --scan --target google.com --ports 80-82 --xyz" 1 "" "Unknown argument"

# 16 — Missing value after --ports
run_test "./wirefish --scan --target google.com --ports" 1 "" "--ports requires"

# 17 — Invalid characters in range
run_test "./wirefish --scan --target google.com --ports abc-80" 1 "" "Invalid number before"