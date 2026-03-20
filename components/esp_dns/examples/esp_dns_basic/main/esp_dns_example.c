/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <unistd.h>
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "lwip/opt.h"
#include "protocol_examples_common.h"
#include "esp_dns.h"
#if defined(CONFIG_MBEDTLS_CERTIFICATE_BUNDLE)
#include "esp_crt_bundle.h"
#endif


#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN    INET_ADDRSTRLEN
#endif

#define TAG "example_esp_dns"

extern const char server_root_cert_pem_start[] asm("_binary_cert_google_root_pem_start");
extern const char server_root_cert_pem_end[]   asm("_binary_cert_google_root_pem_end");


/**
 * @brief Performs DNS lookup for a given hostname and address family
 * @param hostname The hostname to resolve
 * @param family The address family (AF_INET, AF_INET6, or AF_UNSPEC)
 */
static void do_getaddrinfo(char *hostname, int family)
{
    struct addrinfo hints, *res, *p;
    int status;
    char ipstr[INET6_ADDRSTRLEN];
    void *addr = NULL;
    char *ipver = NULL;

    /* Initialize the hints structure */
    memset(&hints, 0, sizeof hints);
    hints.ai_family = family;
    hints.ai_socktype = SOCK_DGRAM; /* UDP datagram sockets */

    /* Get address information */
    if ((status = getaddrinfo(hostname, NULL, &hints, &res)) != 0) {
        ESP_LOGE(TAG, "getaddrinfo error: %d", status);
        goto cleanup;
    }

    /* Loop through all the results */
    for (p = res; p != NULL; p = p->ai_next) {

        /* Get pointer to the address itself */
#if defined(CONFIG_LWIP_IPV4)
        if (p->ai_family == AF_INET) { /* IPv4 */
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            addr = &(ipv4->sin_addr);
            ipver = "IPv4";

            /* Convert the IP to a string and print it */
            inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
            ESP_LOGI(TAG, "Hostname: %s: %s(%s)", hostname, ipstr, ipver);
        }
#endif
#if defined(CONFIG_LWIP_IPV6)
        if (p->ai_family == AF_INET6) { /* IPv6 */
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
            addr = &(ipv6->sin6_addr);
            ipver = "IPv6";

            /* Convert the IP to a string and print it */
            inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
            ESP_LOGI(TAG, "Hostname: %s: %s(%s)", hostname, ipstr, ipver);
        }
#endif
    }

cleanup:
    freeaddrinfo(res); /* Free the linked list */
}


/**
 * @brief Task that performs DNS lookups for various hostnames
 * @param pvParameters Parent task handle for notification
 */
static void addr_info_task(void *pvParameters)
{
    TaskHandle_t parent_handle = (TaskHandle_t)pvParameters;

    do_getaddrinfo("yahoo.com", AF_UNSPEC);
    do_getaddrinfo("www.google.com", AF_INET6);
    do_getaddrinfo("0.0.0.0", AF_UNSPEC);
    do_getaddrinfo("fe80:0000:0000:0000:5abf:25ff:fee0:4100", AF_UNSPEC);

    /* Notify parent task before deleting */
    if (parent_handle) {
        xTaskNotifyGive(parent_handle);
    }
    vTaskDelete(NULL);
}


/**
 * @brief Prints system information including heap and stack usage
 */
void print_system_info(void)
{
    /* Get the free heap size */
    uint32_t free_heap = esp_get_free_heap_size();
    uint32_t min_free_heap = esp_get_minimum_free_heap_size();

    /* Get the stack high water mark for the current task */
    UBaseType_t stack_high_water_mark = uxTaskGetStackHighWaterMark(NULL);

    ESP_LOGI(TAG, "Free Heap: %lu bytes, Min Free Heap: %lu bytes, Stack High Water Mark: %u bytes\n",
             free_heap, min_free_heap, stack_high_water_mark);
}


/**
 * @brief Creates and runs the DNS query task
 */
static void run_dns_query_task(void)
{
    TaskHandle_t task_handle = NULL;
    TaskHandle_t parent_handle = xTaskGetCurrentTaskHandle();
    xTaskCreate(addr_info_task, "AddressInfo", 4 * 1024, parent_handle, 5, &task_handle);

    /* Wait for task to complete */
    if (task_handle != NULL) {
        xTaskNotifyWait(0, 0, NULL, portMAX_DELAY);
    }

    print_system_info();
}


/**
 * @brief Performs DNS queries using UDP protocol
 */
void perform_esp_dns_udp_query(void)
{
    esp_dns_handle_t dns_handle;

    ESP_LOGI(TAG, "Executing UDP DNS");

    /* Initialize with UDP DNS configuration */
    esp_dns_config_t udp_config = {
        .dns_server = "dns.google", /* Google DNS */
    };

    /* Initialize UDP DNS module */
    dns_handle = esp_dns_init_udp(&udp_config);
    if (!dns_handle) {
        ESP_LOGE(TAG, "Failed to initialize UDP DNS module");
        return;
    }

    run_dns_query_task();

    /* Cleanup */
    esp_dns_cleanup_udp(dns_handle);
}


/**
 * @brief Performs DNS queries using TCP protocol
 */
void perform_esp_dns_tcp_query(void)
{
    esp_dns_handle_t dns_handle;

    ESP_LOGI(TAG, "Executing TCP DNS");

    /* Initialize with TCP DNS configuration */
    esp_dns_config_t tcp_config = {
        .dns_server = "dns.google",
        .port = ESP_DNS_DEFAULT_TCP_PORT,
        .timeout_ms = ESP_DNS_DEFAULT_TIMEOUT_MS,
    };

    /* Initialize TCP DNS module */
    dns_handle = esp_dns_init_tcp(&tcp_config);
    if (!dns_handle) {
        ESP_LOGE(TAG, "Failed to initialize TCP DNS module");
        return;
    }

    run_dns_query_task();

    /* Cleanup */
    esp_dns_cleanup_tcp(dns_handle);
}


/**
 * @brief Performs DNS queries using DNS over TLS protocol
 * @param val_type Type of certificate validation ("cert" or "bndl")
 */
void perform_esp_dns_dot_query(char *val_type)
{
    esp_dns_handle_t dns_handle;

    ESP_LOGI(TAG, "Executing DNS over TLS");

    /* Initialize with DNS over TLS configuration */
    esp_dns_config_t dot_config = {
        .dns_server = "dns.google",
        .port = ESP_DNS_DEFAULT_DOT_PORT,
        .timeout_ms = ESP_DNS_DEFAULT_TIMEOUT_MS,
    };

    if (strcmp(val_type, "cert") == 0) {
        dot_config.tls_config.cert_pem = server_root_cert_pem_start;
    } else if (strcmp(val_type, "bndl") == 0) {
        dot_config.tls_config.crt_bundle_attach = esp_crt_bundle_attach;
    }

    /* Initialize DoT DNS module */
    dns_handle = esp_dns_init_dot(&dot_config);
    if (!dns_handle) {
        ESP_LOGE(TAG, "Failed to initialize DoT DNS module");
        return;
    }

    run_dns_query_task();

    /* Cleanup */
    esp_dns_cleanup_dot(dns_handle);
}


/**
 * @brief Performs DNS queries using DNS over HTTPS protocol
 * @param val_type Type of certificate validation ("cert" or "bndl")
 */
void perform_esp_dns_doh_query(char *val_type)
{
    esp_dns_handle_t dns_handle;

    ESP_LOGI(TAG, "Executing DNS over HTTPS");

    /* Initialize with DNS over HTTPS configuration */
    esp_dns_config_t doh_config = {
        .dns_server = "dns.google",
        .port = ESP_DNS_DEFAULT_DOH_PORT,

        .protocol_config.doh_config = {
            .url_path = "/dns-query",
        },
    };

    if (strcmp(val_type, "cert") == 0) {
        doh_config.tls_config.cert_pem = server_root_cert_pem_start;
    } else if (strcmp(val_type, "bndl") == 0) {
        doh_config.tls_config.crt_bundle_attach = esp_crt_bundle_attach;
    }

    /* Initialize DoH DNS module */
    dns_handle = esp_dns_init_doh(&doh_config);
    if (!dns_handle) {
        ESP_LOGE(TAG, "Failed to initialize DoH DNS module");
        return;
    }

    run_dns_query_task();

    /* Cleanup */
    esp_dns_cleanup_doh(dns_handle);
}


void app_main(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_err_t ret = nvs_flash_init();   /* Initialize NVS */
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    /* Test Without ESP_DNS module */
    ESP_LOGI(TAG, "Executing DNS without initializing ESP_DNS module");
    run_dns_query_task();

    /* DNS over UDP Test */
    perform_esp_dns_udp_query();

    /* DNS over TCP Test */
    perform_esp_dns_tcp_query();

    /* DNS over TLS Test with cert */
    perform_esp_dns_dot_query("cert");

    /* DNS over TLS Test with cert bundle */
    perform_esp_dns_dot_query("bndl");

    /* DNS over HTTPS Test with cert */
    perform_esp_dns_doh_query("cert");

    /* DNS over HTTPS Test with cert bundle */
    perform_esp_dns_doh_query("bndl");
}
