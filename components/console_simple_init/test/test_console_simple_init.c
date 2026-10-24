/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include "unity.h"
#include "esp_idf_version.h"
#include "console_simple_init.h"

static int s_argc;
static char s_arg[32];

static int test_cmd(int argc, char **argv)
{
    s_argc = argc;
    s_arg[0] = '\0';
    if (argc > 1 && argv[1] != NULL) {
        strlcpy(s_arg, argv[1], sizeof(s_arg));
    }
    return 0;
}

static esp_err_t test_plugin_register(void)
{
    return console_cmd_register("from_plugin", "plugin command", NULL, test_cmd);
}

CONSOLE_CMD_REGISTER_PLUGIN("test_plugin", test_plugin_register);

typedef struct {
    size_t count;
    bool found;
} plugin_walk_ctx_t;

static bool plugin_walk(const console_cmd_plugin_desc_t *desc, void *ctx)
{
    plugin_walk_ctx_t *walk = ctx;
    walk->count++;
    if (desc->name != NULL && strcmp(desc->name, "test_plugin") == 0) {
        walk->found = true;
    }
    return true;
}

static void run_lifecycle_once(void)
{
    int cmd_ret = -1;

    TEST_ASSERT_NULL(console_cmd_get_repl());
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, console_cmd_start());
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE,
                      console_cmd_register("mycmd", "help", NULL, test_cmd));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, console_cmd_run("mycmd", &cmd_ret));

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, console_cmd_plugin_foreach(NULL, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, console_cmd_register_plugin(NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, console_cmd_register_plugin("not_a_plugin"));

    plugin_walk_ctx_t walk = { 0 };
    TEST_ASSERT_EQUAL(ESP_OK, console_cmd_plugin_foreach(plugin_walk, &walk));
    TEST_ASSERT_TRUE(walk.found);
    TEST_ASSERT_EQUAL(walk.count, console_cmd_plugin_count());

    TEST_ASSERT_EQUAL(ESP_OK, console_cmd_init());
    esp_console_repl_t *repl = console_cmd_get_repl();
    TEST_ASSERT_NOT_NULL(repl);

    TEST_ASSERT_EQUAL(ESP_OK, console_cmd_init());
    TEST_ASSERT_EQUAL_PTR(repl, console_cmd_get_repl());

    TEST_ASSERT_EQUAL(ESP_OK, console_cmd_register("mycmd", "help", NULL, test_cmd));
    cmd_ret = -1;
    TEST_ASSERT_EQUAL(ESP_OK, console_cmd_run("mycmd arg", &cmd_ret));
    TEST_ASSERT_EQUAL(0, cmd_ret);
    TEST_ASSERT_EQUAL(2, s_argc);
    TEST_ASSERT_EQUAL_STRING("arg", s_arg);

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 4, 0)
    TEST_ASSERT_EQUAL(ESP_OK, console_cmd_unregister("mycmd"));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, console_cmd_run("mycmd", &cmd_ret));
#else
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, console_cmd_unregister("mycmd"));
#endif

    TEST_ASSERT_EQUAL(ESP_OK, console_cmd_register_plugin("test_plugin"));
    cmd_ret = -1;
    TEST_ASSERT_EQUAL(ESP_OK, console_cmd_run("from_plugin", &cmd_ret));
    TEST_ASSERT_EQUAL(0, cmd_ret);

#if CONFIG_CONSOLE_SIMPLE_INIT_ENABLE_STOP
    /* Release the console, UART VFS, and command registry so the case can run
     * again. Enabled via CONFIG_CONSOLE_SIMPLE_INIT_ENABLE_STOP in
     * test_app/sdkconfig.defaults. */
    TEST_ASSERT_EQUAL(ESP_OK, console_cmd_stop());
    TEST_ASSERT_NULL(console_cmd_get_repl());
#endif
}

TEST_CASE("console_simple_init lifecycle", "[console_simple_init]")
{
    run_lifecycle_once();
#if CONFIG_CONSOLE_SIMPLE_INIT_ENABLE_STOP
    /* Same boot, second pass. Fails at the NULL-repl asserts if stop() leaked. */
    run_lifecycle_once();
#endif
}
