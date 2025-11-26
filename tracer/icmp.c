/*
 * File: icmp.c
 * Implements ICMP echo packet building and checksum routine.
 *
 * Implementation:
 *  - RFC 1071 checksum for IP/ICMP
 *  - Minimal ICMP header layout for Echo Request/Reply
 */

 #include "icmp.h"

#include <string.h>         // memcpy, memset
#include <arpa/inet.h>      // htons()
#include <netinet/ip_icmp.h> // struct icmphdr, ICMP_ECHO

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