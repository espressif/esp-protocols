/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <stdio.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <net/if.h>
#include <ifaddrs.h>

#ifdef ESP_PLATFORM
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "protocol_examples_common.h"
#else
// Provides simplified support for building on linux (whithout official IDF linux target)
// in order to check with the actual host functionality
// use `gcc main/app.c -lpthread`
#define SIMPLE_LOG( lev, tag, format, ... ) do { printf("%s[%s]: ", lev, tag); printf(format, ##__VA_ARGS__); printf("\n"); } while(0)
#define ESP_LOGE(tag, format, ... ) SIMPLE_LOG("E", tag, format, ##__VA_ARGS__)
#define ESP_LOGI(tag, format, ... ) SIMPLE_LOG("I", tag, format, ##__VA_ARGS__)
#define ESP_ERROR_CHECK(action) action
static void example_connect(void) { }
#endif

static const char *TAG = "sock_utils_example";

static void *reader_thread(void *arg)
{
    int *pipe_fds = (int *)arg;
    char buffer[64];
    int len;
    while ((len = read(pipe_fds[0], buffer, sizeof(buffer))) < 0) {
        if (errno != EAGAIN) {
            ESP_LOGE(TAG, "Failed reading from pipe: %d", errno);
            return NULL;
        }
        usleep(100000);
    }
    buffer[len] = '\0';
    ESP_LOGI(TAG, "Received signal: %s", buffer);
    if (strcmp(buffer, "IP4") != 0) {
        ESP_LOGE(TAG, "Unknown signal");
        return NULL;
    }

    struct ifaddrs *addresses, *addr;
    if (getifaddrs(&addresses) == -1) {
        ESP_LOGE(TAG, "getifaddrs() failed");
        return NULL;
    }
    addr = addresses;

    while (addr) {
        if (addr->ifa_addr && addr->ifa_addr->sa_family == AF_INET) {   // look for IP4 addresses
            struct sockaddr_in *sock_addr = (struct sockaddr_in *) addr->ifa_addr;
            if ((addr->ifa_flags & IFF_UP) == 0) {
                ESP_LOGI(TAG, "Network interface \"%s\" is down", addr->ifa_name);
            } else {
                if (getnameinfo((struct sockaddr *)sock_addr, sizeof(*sock_addr),
                                buffer, sizeof(buffer), NULL, 0, NI_NUMERICHOST) != 0) {
                    freeifaddrs(addresses);
                    return NULL;
                }
                ESP_LOGI(TAG, "IPv4 address of interface \"%s\": %s", addr->ifa_name, buffer);
            }
        }
        addr = addr->ifa_next;
    }
    freeifaddrs(addresses);
    return NULL;
}

static void simple_pipe_example(void)
{
    int pipe_fds[2];
    if (pipe(pipe_fds) == -1) {
        ESP_LOGE(TAG, "Failed to create pipe");
        return;
    }

    // creates reader thread to wait for a pipe signal
    // and print out our IPv4 addresses
    pthread_t reader;
    pthread_create(&reader, NULL, reader_thread, pipe_fds);

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    // at this point we should have received an IP address -> send signal to the reader thread
    const char signal[] = "IP4";
    write(pipe_fds[1], signal, sizeof(signal));
    pthread_join(reader, NULL);

    // Close pipe file descriptors
    close(pipe_fds[0]);
    close(pipe_fds[1]);
}

#ifdef ESP_PLATFORM
void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    simple_pipe_example();
}
#else
int main(void)
{
    simple_pipe_example();
}
#endif
