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

#define ENABLE_NATIVE_UDP 1
#define ENABLE_UDP 1
#define ENABLE_TCP 1
#define ENABLE_DOT_CERT 1
#define ENABLE_DOT_BNDL 1
#define ENABLE_HTTPS_CERT 1
#define ENABLE_HTTPS_BNDL 1


#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN    INET_ADDRSTRLEN
#endif

extern const char server_root_cert_pem_start[] asm("_binary_cert_google_root_pem_start");
extern const char server_root_cert_pem_end[]   asm("_binary_cert_google_root_pem_end");


static const char *TAG = "example_esp_dns";

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

void print_system_info(void)
{
    /* Get the free heap size */
    uint32_t free_heap = esp_get_free_heap_size();
    uint32_t min_free_heap = esp_get_minimum_free_heap_size();

    /* Get the stack high water mark for the current task */
    UBaseType_t stack_high_water_mark = uxTaskGetStackHighWaterMark(NULL);

    ESP_LOGI(TAG, "Free Heap: %d bytes, Min Free Heap: %d bytes, Stack High Water Mark: %d bytes",
             free_heap, min_free_heap, stack_high_water_mark);
}

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

void app_main(void)
{
    esp_dns_handle_t dns_handle;

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

#if ENABLE_NATIVE_UDP
    /* Test Without ESP_DNS module */
    printf("\n");
    ESP_LOGI(TAG, "Executing DNS without initializing ESP_DNS module");
    run_dns_query_task();
#endif /* ENABLE_NATIVE_UDP */

#if ENABLE_UDP
    /*---------- UDP Test ----------*/
    printf("\n");
    ESP_LOGI(TAG, "Executing UDP DNS");

    /* Initialize with UDP DNS configuration */
    esp_dns_config_t udp_config = {
        .protocol = DNS_PROTOCOL_UDP,
        .dns_server = "dns.google", /* Google DNS */
    };

    /* Initialize UDP DNS module */
    dns_handle = esp_dns_init(&udp_config);
    if (!dns_handle) {
        ESP_LOGE(TAG, "Failed to initialize UDP DNS module");
        return;
    }

    run_dns_query_task();

    /* Cleanup */
    esp_dns_cleanup(dns_handle);
#endif /* ENABLE_UDP */


#if ENABLE_TCP
    /*---------- DNS over TCP Test ----------*/
    printf("\n");
    ESP_LOGI(TAG, "Executing DNS over TCP with server cert");
    /* Initialize with DNS over HTTPS configuration */
    esp_dns_config_t tcp_config_cert = {
        .protocol = DNS_PROTOCOL_TCP,
        .dns_server = "dns.google",
        .port = DEFAULT_TCP_PORT,
        .timeout_ms = 5000,
    };

    /* Initialize DoT DNS module */
    dns_handle = esp_dns_init(&tcp_config_cert);
    if (!dns_handle) {
        ESP_LOGE(TAG, "Failed to initialize DoH DNS module");
        esp_dns_cleanup(dns_handle);
        return;
    }

    run_dns_query_task();

    /* Cleanup */
    esp_dns_cleanup(dns_handle);
#endif /* ENABLE_TCP */

#if ENABLE_DOT_CERT
    /*---------- DNS over TLS Test with server cert ----------*/
    printf("\n");
    ESP_LOGI(TAG, "Executing DNS over TLS with server cert");
    /* Initialize with DNS over HTTPS configuration */
    esp_dns_config_t dot_config_cert = {
        .protocol = DNS_PROTOCOL_DOT,
        .dns_server = "dns.google",
        .port = DEFAULT_DOT_PORT,
        .timeout_ms = 5000,

        .tls_config = {
            .cert_pem = server_root_cert_pem_start,
        },
    };

    /* Initialize DoT DNS module */
    dns_handle = esp_dns_init(&dot_config_cert);
    if (!dns_handle) {
        ESP_LOGE(TAG, "Failed to initialize DoH DNS module");
        esp_dns_cleanup(dns_handle);
        return;
    }

    run_dns_query_task();

    /* Cleanup */
    esp_dns_cleanup(dns_handle);
#endif /* ENABLE_DOT_CERT */

#if ENABLE_DOT_BNDL
    /*---------- DNS over TLS Test with cert bundle ----------*/
    printf("\n");
    ESP_LOGI(TAG, "Executing DNS over TLS with cert bundle");
    /* Initialize with DNS over HTTPS configuration */
    esp_dns_config_t dot_config = {
        .protocol = DNS_PROTOCOL_DOT,
        .dns_server = "dns.google",
        .port = DEFAULT_DOT_PORT,
        .timeout_ms = 5000,

        .tls_config = {
            .crt_bundle_attach = esp_crt_bundle_attach
        },
    };

    /* Initialize DoT DNS module */
    dns_handle = esp_dns_init(&dot_config);
    if (!dns_handle) {
        ESP_LOGE(TAG, "Failed to initialize DoH DNS module");
        esp_dns_cleanup(dns_handle);
        return;
    }

    run_dns_query_task();

    /* Cleanup */
    esp_dns_cleanup(dns_handle);
#endif /* ENABLE_DOT_BNDL */

#if ENABLE_HTTPS_CERT
    /*---------- DNS over HTTPS Test with server cert ----------*/
    printf("\n");
    ESP_LOGI(TAG, "Executing DNS over HTTPS with server cert");
    /* Initialize with DNS over HTTPS configuration */
    esp_dns_config_t doh_config_cert = {
        .protocol = DNS_PROTOCOL_DOH,
        .dns_server = "dns.google",
        .port = DEFAULT_DOH_PORT,

        .tls_config = {
            .cert_pem = server_root_cert_pem_start,
        },

        .protocol_config.doh_config = {
            .url_path = "/dns-query",
        },
    };

    /* Initialize DoH DNS module */
    dns_handle = esp_dns_init(&doh_config_cert);
    if (!dns_handle) {
        ESP_LOGE(TAG, "Failed to initialize DoH DNS module");
        esp_dns_cleanup(dns_handle);
        return;
    }

    run_dns_query_task();

    /* Cleanup */
    esp_dns_cleanup(dns_handle);
#endif /* ENABLE_HTTPS_CERT */

#if ENABLE_HTTPS_BNDL
    /*---------- DNS over HTTPS Test with cert bundle ----------*/
    printf("\n");
    ESP_LOGI(TAG, "Executing DNS over HTTPS with cert bundle");

    /* Initialize with DNS over HTTPS configuration */
    esp_dns_config_t doh_config = {
        .protocol = DNS_PROTOCOL_DOH,
        .dns_server = "dns.google",
        .port = DEFAULT_DOH_PORT,

        .tls_config = {
            .crt_bundle_attach = esp_crt_bundle_attach
        },

        .protocol_config.doh_config = {
            .url_path = "/dns-query",
        },
    };

    /* Initialize DoH DNS module */
    dns_handle = esp_dns_init(&doh_config);
    if (!dns_handle) {
        ESP_LOGE(TAG, "Failed to initialize DoH DNS module");
        esp_dns_cleanup(dns_handle);
        return;
    }

    run_dns_query_task();

    /* Cleanup */
    esp_dns_cleanup(dns_handle);
#endif /* ENABLE_HTTPS_BNDL */
}
