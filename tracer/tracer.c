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
 */

#include "tracer.h"
#include "icmp.h"
#include "../net/net.h"
#include "../model/model.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>   // gettimeofday()
#include <arpa/inet.h>  // inet_ntop()
#include <netinet/ip_icmp.h>

#define ICMP_ID  0x1234   // arbitrary unique ID
#define ICMP_SEQ 1        // sequence number

static long now_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000L) + (tv.tv_usec / 1000);
}

static void tracer_append(TraceRoute *route, const Hop *h) {

    if(route->len == route->cap){
        size_t newcap = route->cap ? route->cap * 2 : 16;
        route->rows = realloc(route->rows, newcap * sizeof(Hop));
        route->cap = newcap;
    }

    route->rows[route->len++] = *h;
}

int tracer_run(const CommandLine *cfg, TraceRoute *out){

    //clear TraceRoute to empty
    memset(out, 0, sizeof(*out));

    //resolving target
    struct sockaddr_storage target_addr;
    socklen_t target_len;

    struct sockaddr *sa = (struct sockaddr *)&target_addr;

    if(net_resolve(cfg->target, &target_addr, &target_len) != 0){
        fprintf(stderr, "Error: Failed to resolve target '%s'\n", cfg->target);
        return -1;
    }

    //creating raw ICMP socket
    int sockfd = net_icmp_raw_socket();
    if(sockfd < 0){
        return -1; // error already printed
    }

    //Iterate TTL from cfg->ttl_start → cfg->ttl_max
    for(int ttl = cfg->ttl_start; ttl <= cfg->ttl_max; ttl++){

        net_set_ttl(sockfd, ttl);

        //Build ICMP Echo Request packet
        unsigned char pkt[64];
        size_t pktlen = 0;

        if(icmp_build_echo(ICMP_ID, ttl, NULL, 0, pkt, &pktlen) < 0){
            fprintf(stderr, "Error: ICMP packet build failed\n");
            close(sockfd);
            return -1;
        }

        // Record start time
        long tstart = now_ms();

        //Send ICMP Echo Request
        if(sendto(sockfd, pkt, pktlen, 0, sa, target_len) < 0){
            perror("sendto");
            close(sockfd);
            return -1;
        }

        // prepare buffer to receive response
        char recvbuf[512];
        struct sockaddr_in reply_addr;
        socklen_t reply_len = sizeof(reply_addr);

        //Set timeout (1 second)
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sockfd, &fds);

        int sel = select(sockfd + 1, &fds, NULL, NULL, &tv);

        //initialize Hop
        Hop h;
        memset(&h, 0, sizeof(h));
        h.hop = ttl;

        if(sel == 0){
            // timeout
            h.timeout = true;
            strcpy(h.ip, "*");
            strcpy(h.host, "?");
            h.rtt_ms = -1;

            tracer_append(out, &h);
            continue;
        }

        //recvfrom() for actual ICMP response
        int n = recvfrom(sockfd, recvbuf, sizeof(recvbuf), 0,
                         (struct sockaddr *)&reply_addr, &reply_len);

        long tend = now_ms();

        if(n < 0){
            // treat as timeout
            h.timeout = true;
            strcpy(h.ip, "*");
            strcpy(h.host, "?");
            h.rtt_ms = -1;
            tracer_append(out, &h);
            continue;
        }

        //Extract IP of hop
        inet_ntop(AF_INET, &reply_addr.sin_addr, h.ip, sizeof(h.ip));

        //Parse ICMP type
        int icmp_type = 0;
        icmp_parse_response(recvbuf, n, h.ip, &icmp_type);

        h.timeout = false;
        h.rtt_ms = (int)(tend - tstart);
        strcpy(h.host, h.ip); // could add reverse DNS later

        tracer_append(out, &h);

        //If we reached destination, stop
        if(icmp_type == ICMP_ECHOREPLY){
            break;
        }
    }

    close(sockfd);
    return 0;
}
