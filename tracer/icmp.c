/*
 * File: icmp.c
 * Implements ICMP echo packet building and checksum routine.
 *
 * Implementation:
 *  - RFC 1071 checksum for IP/ICMP
 *  - Minimal ICMP header layout for Echo Request/Reply
 * 
 * Author: Shan Truong - 400576105 - truons8
 * Date: December 3, 2025
 * Coures: 2XC3
 */

#include "icmp.h"
#include <string.h>         // memcpy, memset
#include <arpa/inet.h>      // htons()
#include <netinet/ip_icmp.h> // struct icmphdr, ICMP_ECHO
#include <netinet/ip.h>       // for struct iphdr

uint16_t icmp_checksum(const void *buf, size_t len) {
    const uint16_t *data = (const uint16_t *)buf;
    uint32_t sum = 0;

    // Sum 16-bit words
    while(len > 1) {
        sum += *data++;
        len -= 2;
    }

    // If we have a leftover byte, pad it to 16 bits and add
    if(len == 1) {
        uint16_t last = 0;
        *(uint8_t *)&last = *(const uint8_t *)data;
        sum += last;
    }

    // Fold 32-bit sum to 16 bits: keep adding carry into low 16 bits
    while(sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    // One's complement of the result
    return (uint16_t)(~sum);
}

int icmp_build_echo(uint16_t id, uint16_t seq, const void *payload, size_t payload_len, unsigned char *out, size_t *out_len) {
    if(out == NULL || out_len == NULL) {
        return -1;
    }

    // Total packet length = header + payload
    size_t total_len = sizeof(struct icmphdr) + payload_len;

    // Point header into the output buffer
    struct icmphdr *hdr = (struct icmphdr *)out;
    memset(hdr, 0, sizeof(*hdr));

    hdr->type = ICMP_ECHO;   // 8 = Echo Request
    hdr->code = 0;           // always 0 for Echo
    hdr->un.echo.id = htons(id);
    hdr->un.echo.sequence = htons(seq);

    // Copy payload (if any) right after the header
    if(payload_len > 0 && payload != NULL) {
        memcpy(out + sizeof(struct icmphdr), payload, payload_len);
    }

    // Checksum calculated over entire ICMP message (header + payload)
    hdr->checksum = 0; // must be zeroed before computing checksum
    hdr->checksum = icmp_checksum(out, total_len);

    *out_len = total_len;
    return 0;
}

int icmp_parse_response(const void *packet, size_t len, const char *expected_ip, int *out_type){
    (void)expected_ip; // currently unused, but kept for future filtering

    if(packet == NULL || out_type == NULL){
        return -1;
    }

    const unsigned char *buf = (const unsigned char *)packet;

    // Need at least an IP header (IPv4)
    if(len < sizeof(struct iphdr)){
        *out_type = -1;
        return -1;
    }

    // Interpret the start of the buffer as an IPv4 header
    const struct iphdr *iph = (const struct iphdr *)buf;

    // ihl = "IP header length" in 32-bit words, so multiply by 4 to get bytes
    int iphdr_len = iph->ihl * 4;

    // Make sure we have enough bytes for IP + ICMP header
    if(len < (size_t)(iphdr_len + sizeof(struct icmphdr))){
        *out_type = -1;
        return -1;
    }

    const struct icmphdr *icmph =
        (const struct icmphdr *)(buf + iphdr_len);

    *out_type = icmph->type;
    return 0;
}