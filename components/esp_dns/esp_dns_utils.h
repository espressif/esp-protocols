/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "freertos/FreeRTOS.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_tls.h"
#include "sdkconfig.h"
#include "lwip/prot/dns.h"
#include "lwip/api.h"
#include "lwip/opt.h"
#include "lwip/dns.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BUFFER_SIZE 512

/* DNS header structure */
typedef struct {
    uint16_t id;       /* Identification */
    uint16_t flags;    /* Flags */
    uint16_t qdcount;  /* Number of questions */
    uint16_t ancount;  /* Number of answers */
    uint16_t nscount;  /* Number of authority records */
    uint16_t arcount;  /* Number of additional records */
} dns_header_t;

/* DNS question structure */
typedef struct {
    /* DNS query record starts with either a domain name or a pointer
       to a name already present somewhere in the packet. */
    uint16_t qtype;    /* Question type (e.g., A, MX) */
    uint16_t qclass;   /* Question class (e.g., IN for internet) */
} dns_question_t;

/** DNS answer message structure.
    No packing needed: only used locally on the stack. */
typedef struct {
    /* DNS query record starts with either a domain name or a pointer
       to a name already present somewhere in the packet. */
    uint16_t type;      /* Type (e.g., A, MX) */
    uint16_t class;     /* Class (e.g., IN for internet) */
    uint32_t ttl;       /* Time-to-live */
    uint16_t data_len;  /* Length of data */
} dns_answer_t;
#define SIZEOF_DNS_ANSWER 10

/** DNS resource record max. TTL (one week as default) */
#define DNS_MAX_TTL               604800

#define MAX_ANSWERS (CONFIG_LWIP_DNS_MAX_HOST_IP)

typedef struct {
    err_t status;
    ip_addr_t ip;
} dns_answer_storage_t;

typedef struct {
    err_t status_code;
    uint16_t id;
    int num_answers;
    dns_answer_storage_t answers[MAX_ANSWERS];
} dns_response_t;

/* Buffer structure to store and process DNS response data */
typedef struct {
    char *buffer;          /* Pointer to dynamically allocated buffer for response data */
    int length;            /* Current length of data in the buffer */
    dns_response_t dns_response; /* Parsed DNS response information */
} response_buffer_t;

/* Function to create a DNS query for an A and AAAA records */
size_t create_dns_query(uint8_t *buffer, size_t buffer_size, const char *hostname, int addrtype, uint16_t *id_o);

/* Function to parse a DNS answer for an A and AAAA records */
void parse_dns_response(uint8_t *buffer, size_t response_size, dns_response_t *dns_response);


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
err_t write_ip_addresses_from_dns_response(const dns_response_t *response, ip_addr_t ipaddr[]);

#ifdef __cplusplus
}
#endif
