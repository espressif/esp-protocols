/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "esp_netif.h"
#include "esp_console.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "argtable3/argtable3.h"
#include "esp_log.h"
#include "esp_netif_net_stack.h"
#include "lwip/ip6.h"
#include "lwip/opt.h"
#if IP_NAPT
#include "lwip/lwip_napt.h"
#endif
#include "console_connect.h"
#include "console_ifconfig.h"

static const char *TAG = "console_connect";

static esp_console_repl_t *s_repl = NULL;

/* handle 'quit' command */
static int do_cmd_quit(int argc, char **argv)
{
    printf("Bye Bye\n\r\n");
    s_repl->del(s_repl);
    return 0;
}

static esp_console_cmd_t register_quit(void)
{
    esp_console_cmd_t command = {
        .command = "quit",
        .help = "Quit REPL environment",
        .func = &do_cmd_quit
    };
    return command;
}


esp_err_t example_start_networking_console(char *usr_cmd, int (*usr_cmd_hndl)(int argc, char **argv))
{
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    esp_console_cmd_t command;

    // Initialize TCP/IP network interface aka the esp-netif (should be called only once in application)
    //ESP_ERROR_CHECK(esp_netif_init());
    // Create default event loop that running in background
    //ESP_ERROR_CHECK(esp_event_loop_create_default());

    // install console REPL environment
#if CONFIG_ESP_CONSOLE_UART
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &s_repl));
#endif

#if CONFIG_EXAMPLE_CMD_IFCONFIG
    /* register command `ifconfig` */
    command = register_ifconfig();
    if (esp_console_cmd_register(&command)) {
        ESP_LOGE(TAG, "Unable to register ifconfig");
    }
#endif

#if CONFIG_EXAMPLE_CMD_QUIT
    /* register command `quit` */
    command = register_quit();
    if (esp_console_cmd_register(&command)) {
        ESP_LOGE(TAG, "Unable to register quit");
    }
#endif

    /* Register command from caller */
    if ((usr_cmd_hndl != NULL) && (usr_cmd != NULL)) {
        esp_console_cmd_t command = {
            .command = usr_cmd,
            .help = "user command",
            .func = usr_cmd_hndl
        };
        if (esp_console_cmd_register(&command) != ESP_OK) {
            ESP_LOGE(TAG, "Unable to register user command");
        }
    }

    // start console REPL
    return esp_console_start_repl(s_repl);
}
