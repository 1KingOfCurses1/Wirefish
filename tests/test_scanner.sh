# /*
#  * File: tests/test_scanner.c
#  * Summary: Unit tests for the TCP port-scanning module.
#  *
#  * Purpose:
#  *  - Validate port range parsing and iteration.
#  *  - Verify TCP connect-based classification of OPEN/CLOSED/FILTERED.
#  *  - Ensure timeouts and latency measurements behave deterministically.
#  *
#  * Test Matrix (examples):
#  *  - Localhost happy path:
#  *      cfg.target = "127.0.0.1", ports 1–3 (expect mostly CLOSED)
#  *  - Known-open port (if available):
#  *      cfg.target = "127.0.0.1", port of a test server started by the test (expect OPEN)
#  *  - Timeout behavior:
#  *      cfg.target = "10.255.255.1" (non-routable) or firewall-blocked host (expect FILTERED/timeout)
#  *  - Input validation:
#  *      ports_from > ports_to → expect error return
#  *      ports outside [1..65535] → expect error return
#  *
#  * What is Verified:
#  *  - Return codes (0 on success; <0 on invalid args or runtime failure).
#  *  - For each scanned port: state ∈ {OPEN, CLOSED, FILTERED}.
#  *  - Latency field is either a non-negative ms value (when measured) or -1.
#  *  - Out table allocated and freed without leaks (valgrind clean run).
#  *
#  * Modules Covered:
#  *  - scanner.c / scanner.h
#  *  - net.c (connect with timeout), timeutil.c (latency), config.c (sanity)
#  *
#  * Fixtures / Setup:
#  *  - Optional ephemeral TCP server bound to 127.0.0.1:0 to guarantee an OPEN port.
#  *  - Environment variable override for target/port range (e.g., NETGUARD_TEST_TARGET).
#
# Author: Youssef Khafagy
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

# 1 - simple scan on one known port (just checking table header shows)
run_test "./wirefish --scan --target google.com --ports 80-80" 0 "PORT  STATE" ""

# 2 - multi-port scan, still should output the header line
run_test "./wirefish --scan --target google.com --ports 80-82" 0 "PORT  STATE" ""

# 3 - CSV output should start with the CSV header
run_test "./wirefish --scan --target google.com --ports 80-82 --csv" 0 "port,state,latency_ms" ""

# 4 - JSON output must contain the "results" key
run_test "./wirefish --scan --target google.com --ports 80-82 --json" 0 "\"results\"" ""

# 5 - missing target  cli catches this before scan runs
run_test "./wirefish --scan --ports 1-3" 1 "" "Error: --target required for scan mode"

# 6 - reversed port range parse_range catches it BEFORE cli validates values
run_test "./wirefish --scan --target google.com --ports 10-1" 1 "" "Range start (10) cannot be greater than end (1)"

# 7 - port 0 not allowed cli checks “Ports must be in range”
run_test "./wirefish --scan --target google.com --ports 0-5" 1 "" "Ports must be in range"

# 8 - port >65535  same cli check
run_test "./wirefish --scan --target google.com --ports 1-70000" 1 "" "Ports must be in range"

# 9 - unresolvable host  scanner_run prints this EXACT error
run_test "./wirefish --scan --target nohost123456 --ports 1-3" 1 "" "Error: Failed to resolve target"

# 10 - unroutable IP  all ports filtered checking for the word "filtered"
run_test "./wirefish --scan --target 10.255.255.1 --ports 80-81" 0 "filtered" ""

# 11 - highest valid port (just checking it prints port number)
run_test "./wirefish --scan --target google.com --ports 65535-65535" 0 "65535" ""

# 12 - two modes specified error from cli_parse
run_test "./wirefish --scan --trace --target google.com --ports 80-81" 1 "" "Only one mode"

# 13 - json + csv not allowed
run_test "./wirefish --scan --target google.com --ports 80-82 --json --csv" 1 "" "Cannot use both"

# 14 - missing mode entirely cli error
run_test "./wirefish --target google.com --ports 80-82" 1 "" "Must specify one mode"

# 15 - unknown argument (cli catches it immediately)
run_test "./wirefish --scan --target google.com --ports 80-82 --xyz" 1 "" "Error: Unknown argument '--xyz'"

# 16 - missing value after --ports
run_test "./wirefish --scan --target google.com --ports" 1 "" "Error: --ports requires"

# 17 - invalid characters in port range (abc-80) parse_range catches it
run_test "./wirefish --scan --target google.com --ports abc-80" 1 "" "Invalid number"

# 18 - big valid port range just to check the loop handles lots of ports
run_test "./wirefish --scan --target google.com --ports 1-100" 0 "PORT  STATE" ""

# 19 - CSV output but scanning only one port
run_test "./wirefish --scan --target google.com --ports 443-443 --csv" 0 "port,state,latency_ms" ""

# 20 - JSON output for a single-port scan
run_test "./wirefish --scan --target google.com --ports 443-443 --json" 0 "\"results\"" ""

# 21 - using a direct IP instead of a hostname
run_test "./wirefish --scan --target 8.8.8.8 --ports 80-81" 0 "PORT  STATE" ""

# 22 - scanning localhost (always resolves and works consistently)
run_test "./wirefish --scan --target 127.0.0.1 --ports 1-3" 0 "PORT  STATE" ""

# 23 - lowest allowed port number (boundary test)
run_test "./wirefish --scan --target google.com --ports 1-1" 0 "PORT  STATE" ""

# 24 - highest allowed port number (boundary test)
run_test "./wirefish --scan --target google.com --ports 65535-65535" 0 "65535" ""

# 25 - using TEST-NET-1 (always unroutable always filtered)
run_test "./wirefish --scan --target 192.0.2.1 --ports 80-82" 0 "filtered" ""

# 26 - scanning example.com (ports 1–3 are reliably closed)
run_test "./wirefish --scan --target example.com --ports 1-3" 0 "closed" ""

# 27 - scan a larger range to trigger ScanTable realloc()
run_test "./wirefish --scan --target google.com --ports 1-200" 0 "PORT  STATE" ""

# 28 - valid scan but then an unknown flag at the end
run_test "./wirefish --scan --target google.com --ports 80-82 --badflag" 1 "" "Unknown argument"

# 29 - JSON flag followed by an invalid flag
run_test "./wirefish --scan --target google.com --ports 80-82 --json --nope" 1 "" "Unknown argument"

# 30 - invalid port range (abc-80)
run_test "./wirefish --scan --target google.com --ports abc-80" 1 "" "Invalid number"

# 31 - missing left-hand number in range (-123)
run_test "./wirefish --scan --target google.com --ports -123" 1 "" "Error: Invalid characters in range '-123'"