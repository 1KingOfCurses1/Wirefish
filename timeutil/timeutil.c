/*
 * File: timeutil.c
 * Purpose: Millisecond-precision timing utilities for the project.
 *
 * Provides functions for obtaining current time in ms, sleeping with 
 * ms precision, computing timestamp differences, and formatting timestamps.
 *
 * AUTHOR: Youssef Elshafei
 * DATE: 2025-12-03
 * VERSION: 1.0.0
 */

#include "timeutil.h"
#include <time.h>
#include <sys/time.h>
#include <errno.h>
#include <stdio.h>

/*
 * ms_now
 * Returns current time in milliseconds since the Unix epoch.
 * Returns: timestamp in ms, or -1 on failure.
 */
long ms_now(void) {
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0) {
        return -1;
    }
    return (long)(tv.tv_sec * 1000LL + tv.tv_usec / 1000);
}

/*
 * ms_sleep
 * Sleeps for the given number of milliseconds.
 * ms: number of milliseconds to sleep
 * Returns: 0 on success, -1 on error.
 */
int ms_sleep(int ms) {
    if (ms < 0) {
        return -1;
    }
    
    struct timespec req;
    req.tv_sec = ms / 1000;
    req.tv_nsec = (ms % 1000) * 1000000L;
    
    while (nanosleep(&req, &req) == -1) {
        if (errno != EINTR) {
            return -1;
        }
    }
    return 0;
}

/*
 * ms_diff
 * Computes the difference between two millisecond timestamps.
 * start_ms: starting timestamp
 * end_ms: ending timestamp
 * Returns: end_ms - start_ms
 */
long ms_diff(long start_ms, long end_ms) {
    return end_ms - start_ms;
}

/*
 * format_timestamp
 * Formats the current local time as HH:MM:SS.mmm.
 * buf: output buffer
 * len: size of buffer
 */
void format_timestamp(char *buf, size_t len) {
    struct timeval tv;
    struct tm *tm_info;
    
    gettimeofday(&tv, NULL);
    tm_info = localtime(&tv.tv_sec);
    
    snprintf(buf, len, "%02d:%02d:%02d.%03ld", tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec, tv.tv_usec / 1000);
}