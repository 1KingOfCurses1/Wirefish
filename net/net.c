/*
 * File: net.c
 * Implements address resolution and socket helpers
 *
 * This file provides the implementation for all networking operations used by wirefish
 * It handles DNS resolution, TCP connections with timeouts, TTL settings, and raw ICMP sockets
 *
 * Notes:
 *  - Uses getaddrinfo() for DNS resolution (supports IPv4)
 *  - net_tcp_connect supports timeout via non-blocking and select()
 *  - Focuses on IPv4 (IPv6 support can be added later)
 *
 * Aryan Verma, 400575438, McMaster University
 */

#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/*
 * Provides close() which is used to close sockets
 */
#include <unistd.h>

/*
 * Provides fcntl() which can set socket to non-blocking mode
 * - F_GETFL: get socket flags
 * - F_SETFL: set socket flags
 * - O_NONBLOCK: non-blocking mode flag
 */
#include <fcntl.h>

/*
 * Provides basic system types (required on some systems)
 */
#include <sys/types.h>

/*
 * Provides socket operations (socket(), connect(), setsockopt())
 */
#include <sys/socket.h>

/*
 * Provides internet protocol structures (sockaddr_in, IPPROTO_*)
 */
#include <netinet/in.h>

/*
 * Provides ICMP protocol definitions (IPPROTO_ICMP)
 */
#include <netinet/ip_icmp.h>

/*
 * Library provides getaddrinfo() and related structures
 * - struct addrinfo: hints and results for address resolution
 * - getaddrinfo(): DNS lookup function
 * - freeaddrinfo(): free the results
 * - gai_strerror(): convert getaddrinfo error to string
 */
#include <netdb.h>

/*
 * Library provides inet_ntop() to convert IP addresses to strings (debugging)
 */
#include <arpa/inet.h>

/*
 * Library provides select() function and fd_set structure 
 * select() lets us wait for a socket to become ready with a timeout
 */
#include <sys/select.h>

#include "net.h"

/*
 * Function: net_resolve
 *
 * Resolves a hostname or IP address string into a socket address structure
 *
 * The getaddrinfo() function does DNS lookups
 *
 * How it works:
 *   1. Give it a hostname (ex, "google.com")
 *   2. Give it "hints" about what kind of address you want
 *   3. It returns a linked list of possible addresses
 *   4. We take the first one and copy it
 * 
 * Returns:
 *  - O for success
 *  - -1 for error
 */
int net_resolve(const char *host, struct sockaddr_storage *out, socklen_t *outlen) {
    
     // The hints structure tells getaddrinfo() what kind of address we want
     // Intially set the structs memory block to all zero
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));  
    
    
    // ai_family is the address family (could be IPv4 or IPv6)
    hints.ai_family = AF_INET; // Setting to IPv4
    
    
    // ai_socktype is the type of socket
    // SOCK_STREAM = TCP
    hints.ai_socktype = SOCK_STREAM; 
    
    
    // Getting the address info 
    struct addrinfo *result;
    int status = getaddrinfo(host, NULL, &hints, &result);
    
    // Checking for errors
    if (status != 0) {
        fprintf(stderr, "Error: DNS resolution failed for '%s': %s\n", host, gai_strerror(status));
        return -1;
    }

    // getaddrinfo() returns a linked list of addresses
    
    // Copy the address from result->ai_addr to our output buffer
    // We use sockaddr_storage because it's large enough for any address type
    memcpy(out, result->ai_addr, result->ai_addrlen);
    *outlen = result->ai_addrlen;
    
    freeaddrinfo(result);
    
    return 0; 
}

/*
 * Function: net_tcp_connect
 *
 * Creates a TCP connection with a timeout
 *
 * Port States:
 * - OPEN: Server accepts connection (SYN-ACK received)
 * - CLOSED: Server refuses connection (RST received)
 * - FILTERED: No response (firewall blocking, or host down)
 *
 * Uses Non-Blocking
 *   Normal connect() blocks (waits) until connection succeeds or fails
 *   This could take forever if the host is down
 *   Non-blocking mode lets us set our own timeout
 */
int net_tcp_connect(const struct sockaddr *sa, socklen_t slen, int timeout_ms) {

    // socket() creates an endpoint for communication
    // Specifying to use IPv4 and TCP protocol 
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }
    
    // By default, sockets block on operations like connect()
    // We want non-blocking so we can implement our own timeout
    
    // Get current socket flags
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags < 0) {
        perror("fcntl F_GETFL");
        close(sockfd);
        return -1;
    }
    
    // Add non-blocking flag
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("fcntl F_SETFL");
        close(sockfd);
        return -1;
    }
    
    // connect() initiates the TCP 3-way handshake
    // In non-blocking mode connect() will return immediately
    int result = connect(sockfd, sa, slen);
    
    // Check immediate connection success (rare but possible for localhost)
    if (result == 0) {
        return sockfd;
    }
    
    // For non-blocking sockets, errno should be EINPROGRESS
    // This means connection is in progress
    if (errno != EINPROGRESS) {
        // Real error 
        close(sockfd);
        return -1;
    }
    

    // Use select() to wait for the socket to become writable
    // socket becomes writable when the connection succeeds (we can write data), or connection fails (error pending)
    
     // fd_set: a set of file descriptors to monitor
     // Use FD_ZERO and FD_SET to manipulate this set
    fd_set writefds;
    FD_ZERO(&writefds);          // Clear the set
    FD_SET(sockfd, &writefds);   // Add our socket to the set
    

    // struct timeval: represents timeout
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;              // Convert ms to seconds
    tv.tv_usec = (timeout_ms % 1000) * 1000;    // Remainder as microseconds
    
    // Use select() to wait for the non-blocking connect() to finish
    result = select(sockfd + 1, NULL, &writefds, NULL, &tv);
    
    if (result <= 0) {
        // Timeout or error
        close(sockfd);
        return -1;
    }
    
    // Check if the connection succeeded
    // Must check for pending errors using getsockopt()
    int error = 0;
    socklen_t error_len = sizeof(error);
    
    // getsockopt() gets the socket option
    // SOL_SOCKET: socket level options
    // SO_ERROR: retrieve pending error (if any)
     
    if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &error_len) < 0) {
        // Couldn't get error status
        close(sockfd);
        return -1;
    }
    

    // If error != 0, connection failed
    if (error != 0) {
        // Connection failed
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}

/*
 * Function: net_set_ttl
 *
 * Sets the Time-To-Live (TTL) value on a socket
 * This controls how many router hops a packet can traverse
 *
 * What is TTL?
 *  - Every IP packet has a TTL field (8 bits, so 0-255)
 *  - Each router that handles the packet decrements TTL by 1
 *  - When TTL reaches 0, the router drops the packet and sends an ICMP "Time Exceeded" message back to the sender
 *
 * Why is TTL important?
 * - Prevents routing loops (packets bouncing forever)
 * - Used by traceroute to discover network paths (find addresses)
 */
int net_set_ttl(int sockfd, int ttl) {

    // setsockopt() is used to set options on a socket

    // sockfd is the socket to modify
    if (setsockopt(sockfd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) < 0) {
        perror("setsockopt IP_TTL");
        return -1;
    }
    
    return 0;
}

/*
 * Function: net_icmp_raw_socket
 *
 * Creates a raw ICMP socket for sending/receiving ICMP packets
 * This is needed for implementing traceroute
 *
 * TCP/UDP sockets automatically add protocol headers
 * Raw sockets let you manually build the entire packet
 *   
 * Why we need it:
 * - There's no "normal" ICMP socket type (like TCP or UDP)
 * - We need to manually construct ICMP packets for traceroute
 * - We need to receive ICMP responses from routers
 *
 * Requires root permissions since raw sockets can be used for spoofing, sending malicious packets or perform network attacks 
 *
 */
int net_icmp_raw_socket() {
    
    // socket() with raw ICMP protocol
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
         // Special error message for permission denied
        if (errno == EPERM) {
            fprintf(stderr, "Error: ICMP raw socket requires root privileges\n");
            fprintf(stderr, "       Run with: sudo ./wirefish --trace ...\n");
        } else {
            perror("socket IPPROTO_ICMP");
        }
        return -1;
    }
    
    return sockfd;
}