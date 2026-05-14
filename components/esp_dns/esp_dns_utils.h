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

/** Maximum number of resource records to scan in a response */
#ifndef ESP_DNS_MAX_ANSWER_SCAN
#define ESP_DNS_MAX_ANSWER_SCAN 16
#endif

#define ESP_DNS_BUFFER_SIZE 512

/**
 * @brief Structure to store a complete DNS response
 */
typedef struct {
    err_t status_code;           /* Overall status of the DNS response */
    uint16_t id;                 /* Transaction ID */
    int num_answers;             /* Number of valid answers */
    ip_addr_t answers[MAX_ANSWERS];  /* Array of IP addresses */
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
 * @brief Copies parsed IP addresses from a DNS response to an array.
 *
 * This function retrieves the valid IPv4 and IPv6 addresses that were
 * previously parsed and stored in the DNS response structure, copying
 * them into the provided array.
 *
 * @param response The parsed DNS response
 * @param ipaddr Array to store the copied IP addresses
 *
 * @return err_t ERR_OK on success, or an error code from the response
 */
err_t esp_dns_get_ips_from_response(const dns_response_t *response, ip_addr_t ipaddr[]);

#ifdef __cplusplus
}
#endif
