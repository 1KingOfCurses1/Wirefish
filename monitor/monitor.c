/*
 * File: monitor.c
 * Purpose: Implements network interface bandwidth monitoring.
 *
 * Reads /proc/net/dev to extract RX/TX byte counters, computes
 * instantaneous bit-rates, calculates rolling averages, and stores
 * samples in a dynamically growing MonitorSeries.
 *
 * AUTHOR: Youssef Elshafei
 * DATE:   2025-12-03
 * VERSION: 1.0
 */

#include "monitor.h"
#include "../timeutil/timeutil.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

// Path to Linux kernel network device statistics */
#define PROC_NET_DEV "/proc/net/dev"  
#define WINDOW_SIZE 10

// Global flag modified by signal handler to stop monitoring loop
static volatile int running = 1;

/*
 * Ring buffer structure used to compute rolling averages.
 * values: dynamically allocated array of doubles
 * size:   total capacity
 * head:   index for next insertion
 * count:  number of items currently stored (<= size)
 */
typedef struct {
    double *values;    // Array to store the values
    size_t size;       // Maximum capacity of the buffer
    size_t head;       // Index where next value will be inserted
    size_t count;      // Current number of elements in buffer
} RingBuffer;

/*
 * Creates and initializes a ring buffer of given size.
 * Parameters:
 *   size – number of elements to allocate
 * Returns:
 *   Pointer to a new RingBuffer, or NULL on allocation failure.
 */
static RingBuffer* ringbuf_create(size_t size) {
    // Allocate memory for the ring buffer structure
    RingBuffer *rb = malloc(sizeof(RingBuffer));
    if (!rb) return NULL;
    
    // Allocate memory for the values array and initialize to zero
    rb->values = calloc(size, sizeof(double));
    if (!rb->values) {
        free(rb);  // Clean up if allocation fails
        return NULL;
    }
    
    // Initialize ring buffer properties
    rb->size = size;
    rb->head = 0;    // Start at beginning
    rb->count = 0;   // Buffer is initially empty
    return rb;
}

/*
 * Inserts a new value into the ring buffer.
 * Overwrites the oldest item if the buffer is full.
 */
static void ringbuf_push(RingBuffer *rb, double value) {
    // Store value at current head position
    rb->values[rb->head] = value;
    // Move head to next position, wrapping around if needed
    rb->head = (rb->head + 1) % rb->size;
    // Increase count until buffer is full, then stop counting up
    if (rb->count < rb->size) {
        rb->count++;
    }
}

/*
 * Computes the arithmetic mean of all values stored in the buffer.
 * Returns 0.0 if the buffer is empty.
 */
static double ringbuf_average(const RingBuffer *rb) {
    if (rb->count == 0) return 0.0;
    
    // Sum all values in the buffer
    double sum = 0.0;
    for (size_t i = 0; i < rb->count; i++) {
        sum += rb->values[i];
    }
    // Return average
    return sum / rb->count;
}

/*
 * Frees all memory used by a ring buffer.
 */
static void ringbuf_free(RingBuffer *rb) {
    if (rb) {
        free(rb->values);  // Free the values array
        free(rb);          // Free the structure itself
    }
}

/*
 * Signal handler used to request a stop of the monitoring loop.
 * Sets global flag 'running' to 0.
 */
static void signal_handler(int sig) {
    (void)sig;  // Explicitly ignore unused parameter
    running = 0; // Set flag to break monitoring loop
}
/*
 * Allows external code to stop monitoring (used by API).
 */
void monitor_stop(void) {
    running = 0;
}

/*
 * Reads RX/TX byte counters for the specified interface.
 * Parameters:
 *   iface     – interface name ("eth0", "wlan0", etc.)
 *   rx_bytes  – output pointer for received byte counter
 *   tx_bytes  – output pointer for transmitted byte counter
 * Returns:
 *   0 on success, -1 if interface not found or read fails.
 */
static int read_iface_stats(const char *iface, unsigned long long *rx_bytes, unsigned long long *tx_bytes) {
    // Open the kernel's network statistics file
    FILE *fp = fopen(PROC_NET_DEV, "r");
    if (!fp) {
        perror("Cannot open /proc/net/dev");
        return -1;
    }
    
    char line[256];
    int found = 0;
    
    /* Skip first two header lines in /proc/net/dev
     * Line 1: "Inter-|   Receive..."  
     * Line 2: " face |bytes    packets..." */
    fgets(line, sizeof(line), fp);
    fgets(line, sizeof(line), fp);
    
    /* Parse each interface line in the file
     * Format: iface_name: rx_bytes rx_packets rx_errs ... tx_bytes tx_packets ... */
    while (fgets(line, sizeof(line), fp)) {
        char iface_name[64];
        unsigned long long rx, tx;
        unsigned long long dummy;  // For fields we don't care about
        
        /* Parse the line - we need the first 10 fields:
         * 1: interface name (with colon)
         * 2-9: RX statistics (we only care about bytes)
         * 10: TX bytes */
        int n = sscanf(line, " %63[^:]: %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                       iface_name, &rx, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &tx);
        
        // Check if we found our interface and parsed enough fields
        if (n >= 10 && strcmp(iface_name, iface) == 0) {
            *rx_bytes = rx;
            *tx_bytes = tx;
            found = 1;
            break;
        }
    }
    
    fclose(fp);
    
    if (!found) {
        fprintf(stderr, "Interface '%s' not found in /proc/net/dev\n", iface);
        return -1;
    }
    
    return 0;
}

/*
 * Selects the first non-loopback interface from /proc/net/dev.
 * Parameters:
 *   iface_out – output buffer for detected interface name
 *   len       – length of output buffer
 * Returns:
 *   0 on success, -1 if no suitable interface exists.
 */
static int get_default_interface(char *iface_out, size_t len) {
    FILE *fp = fopen(PROC_NET_DEV, "r");
    if (!fp) {
        return -1;
    }
    
    char line[256];
    
    // Skip header lines
    fgets(line, sizeof(line), fp);
    fgets(line, sizeof(line), fp);
    
    /* Find first non-loopback interface
     * Loopback interface "lo" is excluded as it's for local traffic only */
    while (fgets(line, sizeof(line), fp)) {
        char iface_name[64];
        // Extract just the interface name (before colon)
        if (sscanf(line, " %63[^:]:", iface_name) == 1) {
            // Skip loopback interface
            if (strcmp(iface_name, "lo") != 0) {
                // Copy found interface name to output buffer
                strncpy(iface_out, iface_name, len - 1);
                iface_out[len - 1] = '\0';  // Ensure null termination
                fclose(fp);
                return 0;
            }
        }
    }
    
    fclose(fp);
    return -1;  // No suitable interface found
}

/*
 * Appends an IfaceStats sample into a MonitorSeries, growing
 * the internal buffer as needed. Capacity doubles when full.
 */
static void monitor_append(MonitorSeries *series, const IfaceStats *stats) {
    // Safety check
    if (series == NULL) {
        return;
    }

    // Check if we need to expand the array
    if (series->len == series->cap) {
        // Double the capacity (or start at 16 if currently 0)
        size_t newcap = series->cap ? series->cap * 2 : 16;

        // Reallocate memory with new capacity
        IfaceStats *newbuf = realloc(series->samples, newcap * sizeof(IfaceStats));

        if (newbuf == NULL) {
            // Allocation failed - keep old data but stop growing
            return;
        }
        series->samples = newbuf;
        series->cap = newcap;
    }

    // Add the new stats to the end of the array
    series->samples[series->len++] = *stats;
}

/*
 * Main bandwidth monitoring loop.
 *
 * Parameters:
 *   iface        – interface to monitor (NULL = auto-detect)
 *   interval_ms  – sampling interval in milliseconds
 *   duration_sec – total duration (0 = run indefinitely)
 *   out          – output series to store collected samples
 *
 * Returns:
 *   0 on success, -1 on invalid arguments or setup failure.
 *
 * Side effects:
 *   Installs SIGINT/SIGTERM handlers.
 *   Allocates memory inside 'out' which must be freed
 *   with monitorseries_free().
 */
int monitor_run(const char *iface, int interval_ms, int duration_sec, MonitorSeries *out) {
    // Validate output parameter
    if (out == NULL) {
        return -1;
    }

    char iface_name[64];
    // Initialize output structure to zero
    memset(out, 0, sizeof(*out));
    
    /* Determine which interface to monitor */
    if (iface == NULL) {
        // Auto-detect first non-loopback interface
        if (get_default_interface(iface_name, sizeof(iface_name)) < 0) {
            fprintf(stderr, "Could not auto-detect interface\n");
            return -1;
        }
    } else {
        // Use specified interface
        strncpy(iface_name, iface, sizeof(iface_name) - 1);
        iface_name[sizeof(iface_name) - 1] = '\0';  // Ensure null termination
    }
    
    /* Set up signal handlers for graceful shutdown */
    signal(SIGINT, signal_handler);   // Ctrl+C
    signal(SIGTERM, signal_handler);  // Termination request
    
    /* Create ring buffers for calculating rolling averages */
    RingBuffer *rx_ring = ringbuf_create(WINDOW_SIZE);  // For receive rates
    RingBuffer *tx_ring = ringbuf_create(WINDOW_SIZE);  // For transmit rates
    if (!rx_ring || !tx_ring) {
        fprintf(stderr, "Failed to allocate ring buffers\n");
        ringbuf_free(rx_ring);
        ringbuf_free(tx_ring);
        return -1;
    }
    
    /* Take initial reading to establish baseline */
    unsigned long long prev_rx, prev_tx, curr_rx, curr_tx;
    if (read_iface_stats(iface_name, &prev_rx, &prev_tx) < 0) {
        ringbuf_free(rx_ring);
        ringbuf_free(tx_ring);
        return -1;
    }
    
    /* Initialize timing variables */
    running = 1;
    long start_time = ms_now();  // Get current time in milliseconds
    long end_time = (duration_sec > 0) ? start_time + (duration_sec * 1000) : 0;
    long prev_time = start_time;
    
    /* Main monitoring loop */
    while (running) {
        /* Sleep for the specified interval */
        if (ms_sleep(interval_ms) < 0) {
            break;  // Sleep was interrupted
        }
        
        /* Check if we've exceeded the requested duration */
        long curr_time = ms_now();
        if (end_time > 0 && curr_time >= end_time) {
            break;  // Time's up
        }
        
        /* Read current network statistics */
        if (read_iface_stats(iface_name, &curr_rx, &curr_tx) < 0) {
            continue;  // Skip this iteration if read fails
        }
        
        /* Calculate time difference since last sample (in seconds) */
        long time_delta_ms = ms_diff(prev_time, curr_time);
        double time_delta_sec = time_delta_ms / 1000.0;
        
        // Skip if time difference is invalid
        if (time_delta_sec <= 0) {
            continue;
        }
        
        /* Calculate how many bytes transferred since last sample */
        unsigned long long rx_delta = curr_rx - prev_rx;  // Received bytes delta
        unsigned long long tx_delta = curr_tx - prev_tx;  // Transmitted bytes delta
        
        /* Calculate instantaneous transfer rates in bits per second
         * Multiply by 8 to convert bytes to bits */
        double rx_rate = (rx_delta * 8.0) / time_delta_sec;
        double tx_rate = (tx_delta * 8.0) / time_delta_sec;
        
        /* Update rolling averages with new rates */
        ringbuf_push(rx_ring, rx_rate);
        ringbuf_push(tx_ring, tx_rate);
        
        // Calculate current rolling averages
        double rx_avg = ringbuf_average(rx_ring);
        double tx_avg = ringbuf_average(tx_ring);
        
        /* Package all statistics into a structure */
        IfaceStats stats;
        memset(&stats, 0, sizeof(stats));        
        strncpy(stats.iface, iface_name, sizeof(stats.iface) - 1);
        stats.iface[sizeof(stats.iface) - 1] = '\0';
        stats.rx_bytes = curr_rx;      // Total received bytes
        stats.tx_bytes = curr_tx;      // Total transmitted bytes
        stats.rx_rate_bps = rx_rate;   // Instantaneous receive rate (bps)
        stats.tx_rate_bps = tx_rate;   // Instantaneous transmit rate (bps)
        stats.rx_avg_bps = rx_avg;     // Rolling average receive rate
        stats.tx_avg_bps = tx_avg;     // Rolling average transmit rate

        // Store this sample in the output series
        monitor_append(out, &stats);
        
        /* Update previous values for next iteration */
        prev_rx = curr_rx;
        prev_tx = curr_tx;
        prev_time = curr_time;
    }
    
    /* Clean up allocated resources */
    ringbuf_free(rx_ring);
    ringbuf_free(tx_ring);
    
    return 0;
}

/*
 * Frees all memory owned by a MonitorSeries.
 * Must be called after monitor_run() to avoid memory leaks.
 */
void monitorseries_free(MonitorSeries *series) {
    if (series == NULL) {
        return;
    }

    free(series->samples);   // Free the dynamic array
    series->samples = NULL;  // Prevent dangling pointer
    series->len = 0;         // Reset length
    series->cap = 0;         // Reset capacity
}