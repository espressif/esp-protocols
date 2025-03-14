/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <arpa/inet.h>
#include "esp_random.h"
#include "esp_dns_utils.h"

/* Function to create a DNS query */
size_t create_dns_query(uint8_t *buffer, size_t buffer_size, const char *hostname, int addrtype, uint16_t *id_o)
{
    /*
    Sample DNS Query for example.com (Type A)
    0x00, 0x00,       // Transaction ID
    0x01, 0x00,       // Flags: Standard query
    0x00, 0x01,       // Questions: 1
    0x00, 0x00,       // Answer RRs: 0
    0x00, 0x00,       // Authority RRs: 0
    0x00, 0x00,       // Additional RRs: 0
    0x07, 'e', 'x', 'a', 'm', 'p', 'l', 'e', // QNAME: example.com
    0x03, 'c', 'o', 'm',
    0x00,             // End of QNAME
    0x00, 0x01,       // QTYPE: A (host address)
    0x00, 0x01        // QCLASS: IN (internet)
    */

    dns_header_t *header = (dns_header_t *)buffer;
    memset(buffer, 0, buffer_size);

    /* Set header fields */
    *id_o = (uint16_t)(esp_random() & 0xFFFF); /* Return the id for response validation */
    header->id = htons(*id_o);         /* Random transaction ID */
    header->flags = htons(0x0100);      /* Standard query with recursion */
    header->qdcount = htons(1);         /* One question */

    /* Add the question name */
    uint8_t *qname = buffer + sizeof(dns_header_t);
    const char *dot = hostname;
    while (*dot) {
        const char *next_dot = strchr(dot, '.');
        if (!next_dot) {
            next_dot = dot + strlen(dot);
        }
        uint8_t len = next_dot - dot;
        *qname++ = len;
        /* Check for overflow */
        if ((qname - buffer) > buffer_size) {
            return -1;
        }
        memcpy(qname, dot, len);
        qname += len;
        dot = (*next_dot) ? next_dot + 1 : next_dot;
    }
    *qname++ = 0;  /* Null-terminate the question name */

    /* Set question fields */
    dns_question_t *question = (dns_question_t *)qname;
    question->qtype = htons(addrtype);
    question->qclass = htons(DNS_RRCLASS_IN);

    /* Return the total query size */
    return (qname + sizeof(dns_question_t)) - buffer;
}

/**
 * Skips over a DNS name in a DNS reply message and returns the offset to the end of the name.
 *
 * This function handles both uncompressed labels and compression pointers according to RFC 1035.
 * Reference: RFC 1035, sections 3.1 (Name Space Definitions) and 4.1.4 (Message Compression).
 *
 * @param ptr Pointer to the start of the DNS name in the DNS message.
 * @return The ptr to the end of the DNS name.
 */
static uint8_t *skip_dns_name(uint8_t *ptr)
{
    uint8_t offset = 0;

    /* Loop through each part of the name, handling labels and compression pointers */
    while (ptr[offset] != 0) {
        /* Check if this part is a compression pointer, indicated by the two high bits set to 1 (0xC0) */
        /* RFC 1035, Section 4.1.4: Compression pointers */
        if ((ptr[offset] & 0xC0) == 0xC0) {
            /* Compression pointer is 2 bytes; move offset by 2 and stop */
            offset += 2;
            return ptr + offset; /* End of name processing due to pointer */
        } else {
            /* Otherwise, it's a label
               RFC 1035, Section 3.1: Labels
               - The first byte is the length of this label
               - Followed by 'length' bytes of label content */
            offset += ptr[offset] + 1;  /* Move past this label (1 byte for length + label content) */
        }
    }

    /* RFC 1035, Section 3.1: End of a name is indicated by a zero-length byte (0x00) */
    offset += 1;    /* Move past the terminating zero byte */
    return ptr + offset;
}

void parse_dns_response(uint8_t *buffer, size_t response_size, dns_response_t *dns_response)
{
    /* Validate input buffer */
    assert(buffer != NULL);

    dns_header_t *header = (dns_header_t *)buffer;

    dns_response->status_code = ERR_OK; /* Initialize DNS response code */

    /* Check if there are answers and Transaction id matches */
    int answer_count = ntohs(header->ancount);
    if ((ntohs(header->id) != dns_response->id) || (answer_count == 0)) {
        dns_response->status_code = ERR_VAL; /* DNS response code */
        return;
    }

    /* Ensure only MAX_ANSWERS are processed */
    dns_response->num_answers = (answer_count < MAX_ANSWERS ? answer_count : MAX_ANSWERS);

    /* Skip the header and question section */
    uint8_t *ptr = buffer + sizeof(dns_header_t);

    /* Skip the question name */
    ptr = skip_dns_name(ptr);

    /* Skip the question type and class */
    ptr += sizeof(dns_question_t);

    /* Parse each answer record */
    for (int i = 0; i < dns_response->num_answers; i++) {

        /* Answer fields */
        ptr = skip_dns_name(ptr);

        dns_answer_t *answer = (dns_answer_t *)ptr;
        uint16_t type = ntohs(answer->type);
        uint16_t class = ntohs(answer->class);
        uint32_t ttl = ntohl(answer->ttl);
        uint16_t data_len = ntohs(answer->data_len);

        /* Skip fixed parts of answer (type, class, ttl, data_len) */
        ptr += SIZEOF_DNS_ANSWER;

        /* Validate RR class and ttl */
        if ((class != DNS_RRCLASS_IN) || (ttl > DNS_MAX_TTL)) {
            dns_response->answers[i].status = ERR_VAL;
            goto next_answer;
        }

        /* Initialize status for this answer */
        dns_response->answers[i].status = ERR_OK;

        /* Check the type of answer */
        if (type == DNS_RRTYPE_A && data_len == 4) {
            /* IPv4 Address (A record) */
            memcpy(&dns_response->answers[i].ip, ptr, sizeof(struct in_addr));
            IP_SET_TYPE(&dns_response->answers[i].ip, IPADDR_TYPE_V4);
        } else if (type == DNS_RRTYPE_AAAA && data_len == 16) {
            /* IPv6 Address (AAAA record) */
            memcpy(&dns_response->answers[i].ip, ptr, sizeof(struct in6_addr));
            IP_SET_TYPE(&dns_response->answers[i].ip, IPADDR_TYPE_V6);
        } else {
            dns_response->answers[i].status = ERR_VAL;
        }

next_answer:
        /* Move pointer to next answer */
        ptr += data_len;
    }
}

/**
 * @brief Converts a dns_response_t to an array of IP addresses.
 *
 * This function iterates over the DNS response and extracts valid
 * IPv4 and IPv6 addresses, storing them in the provided array.
 *
 * @param response The DNS response to process.
 * @param ipaddr An array to store the extracted IP addresses.
 *
 * @return err Status of dns response parsing
 */
err_t write_ip_addresses_from_dns_response(const dns_response_t *response, ip_addr_t ipaddr[])
{
    int count = 0;
    memset(ipaddr, 0, DNS_MAX_HOST_IP * sizeof(ip_addr_t));

    if (response->status_code != ERR_OK) {
        return response->status_code;
    }

    /* Iterate over the DNS answers */
    for (int i = 0; i < response->num_answers && count < DNS_MAX_HOST_IP; i++) {
        const dns_answer_storage_t *answer = &response->answers[i];

        /* Check if the answer is valid */
        if (answer->status != ERR_OK) {
            continue;
        }

        ipaddr[count] = answer->ip;
        count++;
    }

    if (count == 0) {
        return ERR_VAL;
    }

    /* Store the number of valid IP addresses */
    return ERR_OK;
}
