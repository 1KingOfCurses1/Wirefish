/*
 * File: cli.c
 * Implements command-line argument parsing and help text
 *
 * This file handles all command-line argument parsing for wirefish.
 *
 * Behavior:
 *  - Populates CommandLine with defaults and parsed values
 *  - Returns 0 on success and <0 on invalid/missing required args
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "cli.h"


/*
 * Function: cli_print_help
 * 
 * Prints usage information to stdout
 * This is shown when the user types --help or makes an error
 */
void cli_print_help() {
    printf("Usage: wirefish [MODE] [OPTIONS]\n\n");
    
    printf("WireFish - Network reconnaissance and monitoring tool\n\n");
    
    printf("Modes (choose one):\n");
    printf("  --scan              TCP port scanning\n");
    printf("  --trace             ICMP traceroute\n");
    printf("  --monitor           Network interface monitoring\n\n");
    
    printf("Scan Options:\n");
    printf("  --target <host>     Target hostname or IP (required)\n");
    printf("  --ports <from-to>   Port range (default: %d-%d)\n\n", DEFAULT_PORTS_FROM, DEFAULT_PORTS_TO);
    
    printf("Trace Options:\n");
    printf("  --target <host>     Target hostname or IP (required)\n");
    printf("  --ttl <start-max>   TTL range (default: %d-%d)\n\n", DEFAULT_TTL_START, DEFAULT_TTL_MAX);
    
    printf("Monitor Options:\n");
    printf("  --iface <name>      Network interface (default: auto-detect)\n");
    printf("  --interval <ms>     Sample interval in milliseconds (default: %d)\n\n", DEFAULT_INTERVAL_MS);
    
    printf("Output Options:\n");
    printf("  --json              Output in JSON format\n");
    printf("  --csv               Output in CSV format\n\n");
    
    printf("Other:\n");
    printf("  --help              Show this help message\n\n");
    
    printf("Examples:\n");
    printf("  wirefish --scan --target google.com --ports 80-443\n");
    printf("  wirefish --trace --target 8.8.8.8 --json\n");
    printf("  wirefish --monitor --iface eth0 --interval 500\n");
}


