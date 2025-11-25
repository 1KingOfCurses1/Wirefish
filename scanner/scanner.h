/*
 * File: scanner.h
 * Summary: Public API for TCP port scanning (and optional host liveness)
 *
 * Responsibilities:
 *  - Scan a host/IP (or CIDR range, if extended) for open/closed/filtered TCP ports
 *  - Optionally measure connect latency per port
 *
 * Data & Types:
 *  - typedef enum PortState 
 *  - typedef struct ScanResult 
 *  - typedef struct ScanTable
 *
 * Function Prototype: 
 *  - int  scanner_run(const Config *cfg, ScanTable *out)
 *  - void scantable_free(ScanTable *t)
 *
 * Inputs:
 *  - cfg->target (host/IP), cfg->ports_from..ports_to, timeout settings
 * Outputs:
 *  - out->rows entries with per-port state (+ optional latency)
 *
 * Returns:
 *  - 0 on success
 *  - -1 on error 
 *
 * Aryan Verma, 400575438, McMaster University
 */

#ifndef SCANNER_H
#define SCANNER_H

#include <stddef.h>
#include <stdbool.h>
#include "../cli/cli.h"  

// Default connection timeout for port scanning (milliseconds)
#define DEFAULT_CONNECT_TIMEOUT_MS 100

// Initial capacity for ScanTable dynamic array
#define INITIAL_TABLE_CAPACITY 100


/*
 * Enum: PortState
 * 
 * Represents the state of a TCP port after scanning
 * 
 * Values:
 *   PORT_CLOSED (0)   - Connection refused (RST received from server)
 *                       Server actively rejected the connection
 *   
 *   PORT_OPEN (1)     - Connection succeeded (SYN-ACK received)
 *                       Service is listening on this port
 *   
 *   PORT_FILTERED (2) - Connection timed out (no response)
 *                       Firewall blocking, host down, or service not responding
 *
 * Why these three states?
 *   - OPEN: Server accepted connection (interesting for security/inventory)
 *   - CLOSED: Server exists but port is closed (still useful info)
 *   - FILTERED: Can't tell if port is open (firewall/timeout)
 */
typedef enum { 
    PORT_CLOSED = 0,
    PORT_OPEN = 1,     
    PORT_FILTERED = 2  
} PortState;

/*
 * Struct: ScanResult
 * 
 * Represents the result of scanning a single port
 * 
 * Fields:
 *   port        - Port number (1-65535)
 *   state       - PortState (OPEN, CLOSED, or FILTERED)
 *   latency_ms  - Time taken to connect in milliseconds
 *                 Set to -1 if not measured or connection failed
 */
typedef struct {
    int port;        
    PortState state;    
    int latency_ms;     
} ScanResult;

/*
 * Struct: ScanTable
 * 
 * A dynamic array of ScanResults
 * Stores all the scan results for a target
 * 
 * Fields:
 *   rows - Pointer to array of ScanResult structures
 *   len  - Number of results currently stored
 *   cap  - Capacity of the array (for dynamic growth)
 *
 * Memory management:
 *   - scanner_run() allocates memory for rows
 *   - scantable_free() must be called to free memory
 */
typedef struct {
    ScanResult *rows;   
    size_t len;         
    size_t cap;         
} ScanTable;

int scanner_run(const CommandLine *cfg, ScanTable *out);
void scantable_free(ScanTable *t);

#endif


