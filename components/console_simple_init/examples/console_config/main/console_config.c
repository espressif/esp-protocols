/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <stdio.h>
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "argtable3/argtable3.h"
#include "console_simple_init.h"

static const char *TAG = "console_config";

/* Kept for the life of the program: do_echo() parses against it after app_main() returns.
 * Registration only needs it long enough to build the command hint. */
static struct {
    struct arg_str *text;
    struct arg_end *end;
} s_echo_args;

/* "user" command: no arguments. Registered with an explicit help string. */
int do_user_cmd(int argc, char **argv)
{
    (void)argc;
    (void)argv;
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
    (void)argc;
    (void)argv;
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

void app_main(void)
{
    /* Console history and some IDF services expect NVS and the default event loop. */
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Zero means "keep the IDF default". Only the prompt is overridden, so this
     * is console_cmd_init() with "cfg> " instead of "esp> ". */
    console_cmd_config_t config = CONSOLE_CMD_CONFIG_DEFAULT();
    config.prompt = "cfg> ";

    ESP_ERROR_CHECK(console_cmd_init_with_config(&config));
    /* get_repl() is NULL when init did not create a REPL. */
    if (console_cmd_get_repl() == NULL) {
        ESP_LOGE(TAG, "console_cmd_get_repl() returned NULL after init");
        return;
    }

    /* Help text is shown by the "help" command. NULL hint: this command takes no args. */
    ESP_ERROR_CHECK(console_cmd_register("user", "Print a greeting", NULL, do_user_cmd));

    /* arg_str1 makes <text> mandatory. arg_end(2) is the error slot count for arg_parse().
     * hint is generated from this table because register_with_args() does not take one. */
    s_echo_args.text = arg_str1(NULL, NULL, "<text>", "text to print");
    s_echo_args.end = arg_end(2);
    ESP_ERROR_CHECK(console_cmd_register_with_args("echo", "Print an argument", &s_echo_args, do_echo));

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    /* Context commands exist from IDF 5.3. "from-context" is a string literal, so it
     * stays valid for as long as the command is registered. */
    ESP_ERROR_CHECK(console_cmd_register_with_context("ctx", "Print the command context", NULL,
                                                      do_ctx, (void *)"from-context"));
#endif

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
    /* Unregister exists from IDF 5.4. Register "temp", remove it, then confirm run()
     * cannot find it. ESP_ERR_NOT_FOUND is the expected result, so this is not ESP_ERROR_CHECK(). */
    ESP_ERROR_CHECK(console_cmd_register("temp", "Temporary command", NULL, do_user_cmd));
    ESP_ERROR_CHECK(console_cmd_unregister("temp"));
    int missing = 0;
    esp_err_t missing_ret = console_cmd_run("temp", &missing);
    if (missing_ret != ESP_ERR_NOT_FOUND) {
        ESP_LOGE(TAG, "expected temp to be unregistered, got %s", esp_err_to_name(missing_ret));
    }
#endif

    /* Run "user" once before the prompt appears. cmd_ret is do_user_cmd()'s return value.
     * The same command remains available after the REPL starts. */
    int cmd_ret = -1;
    ESP_ERROR_CHECK(console_cmd_run("user", &cmd_ret));

    /* Missing name must fail and must not abort. */
    esp_err_t missing_plugin = console_cmd_register_plugin("not_a_plugin");
    if (missing_plugin != ESP_ERR_NOT_FOUND) {
        ESP_LOGE(TAG, "expected missing plugin, got %s", esp_err_to_name(missing_plugin));
    }

    ESP_LOGI(TAG, "Plugin count: %u", (unsigned)console_cmd_plugin_count());

    /* Walks .console_cmd_desc and calls example_plugin_register(), which adds "plug". */
    ESP_ERROR_CHECK(console_cmd_all_register());
    /* Starts the background REPL task and returns. The prompt is "cfg> ". */
    ESP_ERROR_CHECK(console_cmd_start());
}
