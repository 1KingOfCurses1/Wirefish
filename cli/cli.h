/*
 * File: cli.h
 * Summary: Command-line parsing (--scan, --trace, --monitor, flags)
 *
 * Responsibilities:
 *  - Parse argc and argv into a CommandLine struct
 *  - Validate args and provide help text
 *
 * Function prototypes:
 *  - int cli_parse(int argc, char *argv[], CommandLine *out)
 *  - void cli_print_help()
 * 
 * Aryan Verma, 400575438, McMaster University 
 */
#ifndef CLI_H
#define CLI_H

#include <stdbool.h>


// These are the values used when the user doesn't specify certain options
#define DEFAULT_PORTS_FROM 1        
#define DEFAULT_PORTS_TO 1024       
#define DEFAULT_TTL_START 1        
#define DEFAULT_TTL_MAX 30          
#define DEFAULT_INTERVAL_MS 100   

// These define the valid ranges for various parameters
// Ports: TCP/UDP ports range from 1 to 65535
// TTL: IP Time-To-Live field is 8 bits, so 1-255
#define MIN_PORT 1
#define MAX_PORT 65535
#define MIN_TTL 1
#define MAX_TTL 255



/*
 * Struct: CommandLine
 * 
 * This structure holds all the parsed command-line arguments in one place
 *
 * OUTPUT FORMAT:
 *   json      - true if user wants JSON output format
 *   csv       - true if user wants CSV output format
 *
 * TARGET and INTERFACE:
 *   target    - The hostname or IP address to scan/trace (ex "google.com", "192.168.1.1")
 *               Size 256 is plenty for any valid hostname (max DNS name is 253 chars)
 *   iface     - Network interface name for monitoring (ex "eth0", "wlan0")
 *               Size 64 is standard for interface names (typically much shorter)
 *
 * PORT SCANNING:
 *   ports_from - Starting port number (ex 80)
 *   ports_to   - Ending port number (ex 443)
 *                Valid ports are 1-65535 (TCP/UDP port range)
 *
 * TRACEROUTE TTL:
 *   ttl_start  - Starting Time-To-Live value (usually 1)
 *   ttl_max    - Maximum TTL to try (usually 30)
 *                TTL determines how many "hops" a packet can make
 *
 * MONITORING:
 *   interval_ms - How often to sample network stats, in milliseconds
 *                 (ex, 100 = sample once per second)
 *
 * MODE:
 *   mode - Which operation mode the user selected (scan, trace, or monitor)
 *          We use an enum to make this type-safe and clear
 */

typedef struct {
  bool json, csv;

  char target[256];

  char iface[64];

  int ports_from, ports_to;

  int ttl_start, ttl_max;

  int interval_ms;

  enum {
    MODE_NONE=0, 
    MODE_SCAN,  
    MODE_TRACE,  
    MODE_MONITOR 
  } mode;

} CommandLine;

int cli_parse(int argc, char *argv[], CommandLine *out);
void cli_print_help();

#endif 
