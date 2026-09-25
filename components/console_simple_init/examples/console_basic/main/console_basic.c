/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <stdio.h>
#include "sdkconfig.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "argtable3/argtable3.h"
#include "console_simple_init.h"

static const char *TAG = "console_basic";

/* Kept for the life of the program: do_echo() parses against it after app_main() returns.
 * Registration only needs it long enough to build the command hint. */
static struct {
    struct arg_str *text;
    struct arg_end *end;
} s_echo_args;

int do_user_cmd(int argc, char **argv)
{
    printf("Hello from user command.\n");
    return 0;
}

/* "echo <text>" command. A non-zero return is the command's own failure, not an
 * esp_err_t from the console. */
int do_echo(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&s_echo_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, s_echo_args.end, argv[0]);
        return 1;
    }
    printf("%s\n", s_echo_args.text->sval[0]);
    return 0;
}

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
/* "ctx" command. The first argument is the pointer passed at registration. */
int do_ctx(void *context, int argc, char **argv)
{
    printf("Context: %s\n", (const char *)context);
    return 0;
}
#endif

/* Linked into .console_cmd_desc. console_cmd_all_register() calls this. */
static esp_err_t example_plugin_register(void)
{
    return console_cmd_register("plug", "Registered by the example plugin", NULL, do_user_cmd);
}

CONSOLE_CMD_REGISTER_PLUGIN("example_plugin", example_plugin_register);

static void init_nvs_and_event_loop(void)
{
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

/* Default init, the basic register helpers, and one non-interactive command.
 * console_cmd_start() is not called here: it would own the console until reset
 * and a second init would be a no-op. */
static void run_basic_phase(void)
{
    int cmd_ret = -1;

    ESP_ERROR_CHECK(console_cmd_init());
    ESP_ERROR_CHECK(console_cmd_user_register("user", do_user_cmd));
    ESP_ERROR_CHECK(console_cmd_all_register());
    ESP_ERROR_CHECK(console_cmd_run("user", &cmd_ret));
}

/* Custom prompt, help text, argtable, context, unregister, and the plugin APIs.
 * Ends in the interactive REPL. */
static void run_config_phase(void)
{
    console_cmd_config_t config = CONSOLE_CMD_CONFIG_DEFAULT();
    config.prompt = "cfg> ";

    ESP_ERROR_CHECK(console_cmd_init_with_config(&config));
    if (console_cmd_get_repl() == NULL) {
        ESP_LOGE(TAG, "console_cmd_get_repl() returned NULL after init");
        return;
    }

    ESP_ERROR_CHECK(console_cmd_register("user", "Print a greeting", NULL, do_user_cmd));

    s_echo_args.text = arg_str1(NULL, NULL, "<text>", "text to print");
    s_echo_args.end = arg_end(2);
    ESP_ERROR_CHECK(console_cmd_register_with_args("echo", "Print an argument", &s_echo_args, do_echo));

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    ESP_ERROR_CHECK(console_cmd_register_with_context("ctx", "Print the command context", NULL,
                                                      do_ctx, (void *)"from-context"));
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
    ESP_ERROR_CHECK(console_cmd_register("temp", "Temporary command", NULL, do_user_cmd));
    ESP_ERROR_CHECK(console_cmd_unregister("temp"));
    int missing = 0;
    esp_err_t missing_ret = console_cmd_run("temp", &missing);
    if (missing_ret != ESP_ERR_NOT_FOUND) {
        ESP_LOGE(TAG, "expected temp to be unregistered, got %s", esp_err_to_name(missing_ret));
    }
#endif

    esp_err_t missing_plugin = console_cmd_register_plugin("not_a_plugin");
    if (missing_plugin != ESP_ERR_NOT_FOUND) {
        ESP_LOGE(TAG, "expected missing plugin, got %s", esp_err_to_name(missing_plugin));
    }

    ESP_LOGI(TAG, "Plugin count: %u", (unsigned)console_cmd_plugin_count());
    ESP_ERROR_CHECK(console_cmd_all_register());
    ESP_ERROR_CHECK(console_cmd_start());
}

void app_main(void)
{
    init_nvs_and_event_loop();
    run_basic_phase();

#if CONFIG_CONSOLE_SIMPLE_INIT_ENABLE_STOP
    /* Deletes the REPL and the command list so the configured init below is a
     * new console, not a no-op on the one phase 1 created. */
    ESP_ERROR_CHECK(console_cmd_stop());
#else
    ESP_LOGE(TAG, "console_cmd_stop() requires CONFIG_CONSOLE_SIMPLE_INIT_ENABLE_STOP");
    return;
#endif

    run_config_phase();
}
