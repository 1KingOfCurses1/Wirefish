/*
 * File: scanner.h
 * Summary: Public API for TCP port scanning (and optional host liveness)
 *
 * Responsibilities:
 *  - Scan a host/IP (or CIDR range, if extended) for open/closed/filtered TCP ports
 *  - Optionally measure connect latency per port
 *
 * Data & Types:
 *  - typedef enum PortState { PORT_CLOSED=0, PORT_OPEN=1, PORT_FILTERED=2 }
 *  - typedef struct ScanResult { int port; PortState state; int latency_ms; }
 *  - typedef struct ScanTable { ScanResult *rows; size_t len, cap; }
 *
 * Public API:
 *  - int  scanner_run(const Config *cfg, ScanTable *out);
 *  - void scantable_free(ScanTable *t);
 *
 * Inputs:
 *  - cfg->target (host/IP), cfg->ports_from..ports_to, timeout settings
 * Outputs:
 *  - out->rows entries with per-port state (+ optional latency)
 *
 * Returns:
 *  - 0 on success
 *  - -1 on error (invalid args, network unreachable, etc.)
 *
 * Thread-safety: stateless API; safe to call from multiple threads if 'out' is distinct.
 * Dependencies: config.h, net.h
 *
 * Notes:
 *  - Uses non-blocking connect or timeouts for responsiveness.
 *  - Extend later for parallel scanning or CIDR enumeration.
 *
 * Aryan Verma, 400575438, McMaster University
 */

#include <stddef.h>
#include <stdbool.h>
#include "../cli/cli.h"  

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
    PORT_CLOSED = 0,   // Connection refused
    PORT_OPEN = 1,     // Connection succeeded
    PORT_FILTERED = 2  // Connection timed out
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
    int port;           // Port number
    PortState state;    // Port state (OPEN/CLOSED/FILTERED)
    int latency_ms;     // Connection latency (-1 if failed/not measured)
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
    ScanResult *rows;   // Dynamic array of results
    size_t len;         // Number of results
    size_t cap;         // Capacity (for growth)
} ScanTable;

int scanner_run(const CommandLine *cfg, ScanTable *out);
void scantable_free(ScanTable *t);

