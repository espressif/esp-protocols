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


/**
 * @brief DNS header structure
 *
 * Contains the basic fields of a DNS message header as defined in RFC 1035
 */
typedef struct {
    uint16_t id;       /* Identification - unique identifier for the query */
    uint16_t flags;    /* Flags - control bits for the DNS message */
    uint16_t qdcount;  /* Number of questions in the question section */
    uint16_t ancount;  /* Number of answers in the answer section */
    uint16_t nscount;  /* Number of authority records in the authority section */
    uint16_t arcount;  /* Number of additional records in the additional section */
} dns_header_t;

/**
 * @brief DNS question structure
 *
 * Represents a single question in the question section of a DNS message
 */
typedef struct {
    uint16_t qtype;    /* Question type (e.g., A, AAAA, MX) */
    uint16_t qclass;   /* Question class (e.g., IN for internet) */
} dns_question_t;

/**
 * @brief DNS answer message structure
 *
 * Represents a single resource record in the answer section of a DNS message
 * No packing needed as it's only used locally on the stack
 */
typedef struct {
    uint16_t type;      /* Resource record type (e.g., A, AAAA, MX) */
    uint16_t class;     /* Resource record class (e.g., IN for internet) */
    uint32_t ttl;       /* Time-to-live in seconds */
    uint16_t data_len;  /* Length of the resource data */
} dns_answer_t;

#define SIZEOF_DNS_ANSWER_FIXED 10  /* Size of dns_answer_t structure in bytes */

/** Maximum TTL value for DNS resource records (one week) */
#define DNS_MAX_TTL 604800

#ifndef CONFIG_LWIP_DNS_MAX_HOST_IP
#define CONFIG_LWIP_DNS_MAX_HOST_IP 1
#endif

/** Maximum number of answers that can be stored */
#define MAX_ANSWERS (CONFIG_LWIP_DNS_MAX_HOST_IP)

#define ESP_DNS_BUFFER_SIZE 512

/**
 * @brief Structure to store a single DNS answer
 */
typedef struct {
    err_t status;      /* Status of the answer */
    ip_addr_t ip;      /* IP address from the answer */
} dns_answer_storage_t;

/**
 * @brief Structure to store a complete DNS response
 */
typedef struct {
    err_t status_code;           /* Overall status of the DNS response */
    uint16_t id;                 /* Transaction ID */
    int num_answers;             /* Number of valid answers */
    dns_answer_storage_t answers[MAX_ANSWERS];  /* Array of answers */
} dns_response_t;

/**
 * @brief Buffer structure for DNS response processing
 */
typedef struct {
    char *buffer;                /* Pointer to response data buffer */
    int length;                  /* Current length of data in buffer */
    dns_response_t dns_response; /* Parsed DNS response information */
} response_buffer_t;

/**
 * @brief Creates a DNS query for A and AAAA records
 *
 * @param buffer Buffer to store the query
 * @param buffer_size Size of the buffer
 * @param hostname Hostname to query
 * @param addrtype Address type (A or AAAA)
 * @param id_o Pointer to store the generated query ID
 *
 * @return size_t Size of the created query, or -1 on error
 */
size_t esp_dns_create_query(uint8_t *buffer, size_t buffer_size, const char *hostname, int addrtype, uint16_t *id_o);

/**
 * @brief Parses a DNS response message
 *
 * @param buffer Buffer containing the DNS response
 * @param response_size Size of the response
 * @param dns_response Structure to store parsed response
 */
void esp_dns_parse_response(uint8_t *buffer, size_t response_size, dns_response_t *dns_response);

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
err_t esp_dns_extract_ip_addresses_from_response(const dns_response_t *response, ip_addr_t ipaddr[]);

#ifdef __cplusplus
}
#endif
