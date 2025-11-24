/*
 * File: tracer.c
 * Implements TTL-stepped ICMP echo probes and hop collection.
 *
 * Implementation:
 *  - Raw socket sendto() ICMP ECHO with TTL=ttl
 *  - recvfrom() ICMP TIME_EXCEEDED or ECHO_REPLY
 *  - Measure RTT with timeutil; resolve IP to hostname optionally
 *
 * Requirements:
 *  - Root or CAP_NET_RAW for raw sockets
 *
 * Error Handling:
 *  - Clean teardown on signal/interrupts; timeouts per hop
 * 
 * Author: Shan Truong - 400576105 - truons8
 * Date: December 3, 2025
 * Coures: 2XC3
 */

 // tracer.c
#include "tracer.h"

int tracer_run(const CommandLine *cfg, TraceRoute *out){
    (void)cfg; (void)out;
    return -1; // stub
}

void traceroute_free(TraceRoute *t){
    (void)t;
}
