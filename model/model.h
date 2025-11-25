/*
 * File: model.h
 * Summary: Shared data models used by fmt/* and public APIs.
 *
 * Purpose:
 *  - Centralize the common structs so formatters can include a single header
 *  - Avoid circular includes between feature modules
 *
 * Contains:
 *  - typedefs mirrored from scanner.h (ScanResult, ScanTable)
 *  - typedefs mirrored from tracer.h  (Hop, TraceRoute)
 *  - typedefs mirrored from monitor.h (IfaceStats, MonitorSeries)
 *
 * Note:
 *  - Keep in sync with feature headers or include them conditionally.
 * 
 * Youssef Khafagy
 */
#ifndef MODEL_H
#define MODEL_H

#include <stddef.h>
#include <stdbool.h>

typedef enum { PORT_CLOSED = 0, PORT_OPEN = 1, PORT_FILTERED = 2 } PortState;

typedef struct {
    int port;
    PortState state; //or just int if you haven't switched yet
    int latency_ms;
} ScanResult;

typedef struct {
    ScanResult *rows;
    size_t len, cap; //note: check with team if size_t is preferred over int since int might be too small for very large scans
} ScanTable;

typedef struct {
    int  hop;
    char host[256];
    char ip[64];
    int  rtt_ms;  //-1 if timeout
    bool timeout;
} Hop;

typedef struct {
    Hop *rows;
    size_t len, cap;
} TraceRoute;


typedef struct {

    char iface[64]; //interface name
    long rx_bytes; //total received bytes
    long tx_bytes; //total sent bytes
    double rx_rate_bps;
    double tx_rate_bps;
    double rx_avg_bps;
    double tx_avg_bps;
} IfaceStats;

typedef struct {
    IfaceStats *samples; //array of samples
    size_t len, cap;   // number stored + capacity

} MonitorSeries;


#endif