/* Modem console example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include <esp_modem_dce_common_commands.h>
#include "sdkconfig.h"
#include "esp_console.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "argtable3/argtable3.h"
#include "esp_modem.h"
#include "esp_log.h"

// utilities to check network connectivity
void modem_console_register_http(void);
void modem_console_register_ping(void);

static esp_modem_dce_t *s_dce = NULL;
static const char *TAG = "modem_console";

static struct {
    struct arg_str *command;
    struct arg_int *param_int;
    struct arg_str *param_str;
    struct arg_str *param_pdp;
    struct arg_str *param_bool;
    struct arg_str *param;
    struct arg_str *result;
    struct arg_end *end;
} at_args;

static struct {
    struct arg_str *param;
    struct arg_end *end;
} modem_args;



static struct {
    struct arg_str *command;
    struct arg_int *timeout;
    struct arg_str *pattern;
    struct arg_lit *no_cr;
    struct arg_end *end;
} generic_at_args;

static char s_common_in_str[100];       // used as common string input param holder
static char s_common_out_str[100];      // used as output string/command result holder

static esp_err_t handle_line_pattern(esp_modem_dce_t *dce, const char *line)
{
    esp_err_t err = ESP_OK;
    ESP_LOGI(TAG, "handle_line_pattern: DCE response: %s\n", line);
    if (strstr(line, dce->handle_line_ctx)) {
        err = esp_modem_process_command_done(dce, ESP_MODEM_STATE_SUCCESS);
    }
    return err;
}

static int do_dce(int argc, char **argv)
{
    // specific DCE generic command params
    static bool bool_result;
    static char pdp_type[10];
    static char pdp_apn[10];
    static esp_modem_dce_pdp_ctx_t pdp = { .type = pdp_type, .apn = pdp_apn };
    static esp_modem_dce_csq_ctx_t csq;
    static esp_modem_dce_cbc_ctx_t cbc;

    int nerrors = arg_parse(argc, argv, (void **) &at_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, at_args.end, argv[0]);
        return 1;
    }
    void * command_param = NULL;
    void * command_result = NULL;

    // parse input params
    if (at_args.param_int->count > 0) {
        command_param = (void*)(at_args.param_int->ival[0]);
    } else if (at_args.param_bool->count > 0) {
        const char * bool_in_str = at_args.param_bool->sval[0];
        s_common_out_str[0] = '\0';
        command_result = s_common_out_str;
        if (strstr(bool_in_str,"true") || strstr(bool_in_str,"1")) {
            command_param = (void*)true;
        } else {
            command_param = (void*)false;
        }
    } else if (at_args.param_pdp->count > 0) {
        // parse out three comma separated sub-arguments
        sscanf(at_args.param_pdp->sval[0], "%d,%s", &pdp.cid, pdp_type);
        char *str_apn = strchr(pdp_type, ',');
        if (str_apn) {
            strncpy(pdp_apn, str_apn + 1, sizeof(pdp_apn));
            str_apn[0] = '\0';
        }
        command_param = &pdp;
    } else if (at_args.param_str->count > 0) {
        strncpy(s_common_in_str, at_args.param_str->sval[0], sizeof(s_common_in_str));
        command_param = s_common_in_str;
    } else if (at_args.param->count > 0) {      // default param is treated as string
        strncpy(s_common_in_str, at_args.param->sval[0], sizeof(s_common_in_str));
        command_param = s_common_in_str;
    }

    // parse output params
    if (at_args.result->count > 0) {
        const char *res = at_args.result->sval[0];
        if (strstr(res, "csq")) {
            command_result = &csq;
        }else if (strstr(res, "cbc")) {
            command_result = &cbc;
        } else if (strstr(res, "str")) {
            command_param = (void*)sizeof(s_common_out_str);
            command_result = s_common_out_str;
        } else if (strstr(res, "bool")) {
            command_result = &bool_result;
        } else {
            command_param = (void*)sizeof(s_common_out_str);
            command_result = s_common_out_str;
        }
    }

    // by default (if no param/result provided) expect string output
    if (command_param == NULL && command_result == NULL) {
        s_common_out_str[0] = '\0';
        command_param = (void*)sizeof(s_common_out_str);
        command_result = s_common_out_str;
    }

    esp_err_t err = esp_modem_command_list_run(s_dce, at_args.command->sval[0], command_param, command_result);
    if (err == ESP_OK) {
        printf("Command %s succeeded\n", at_args.command->sval[0]);
        if (command_result == s_common_out_str && s_common_out_str[0] != '\0') {
            ESP_LOGI(TAG, "Command string output: %s", s_common_out_str);
        } else if (command_result == &csq) {
            ESP_LOGI(TAG, "Command CSQ output: rssi:%d, ber:%d", csq.rssi, csq.ber);
        } else if (command_result == &cbc) {
            ESP_LOGI(TAG, "Command battery output:%d mV", cbc.battery_status);
        } else if (command_result == &bool_result) {
            ESP_LOGI(TAG, "Command bool output: %s", bool_result ? "true" : "false");
        }
        return 0;
    }
    ESP_LOGE(TAG, "Command %s failed with %d", at_args.command->sval[0], err);
    return 1;
}



static int do_at_command(int argc, char **argv)
{
    int timeout = 1000;
    int nerrors = arg_parse(argc, argv, (void **)&generic_at_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, generic_at_args.end, argv[0]);
        return 1;
    }
    esp_modem_dce_handle_line_t handle_line = esp_modem_dce_handle_response_default;

    strncpy(s_common_in_str, generic_at_args.command->sval[0], sizeof(s_common_in_str) - 2);
    if (generic_at_args.no_cr->count == 0) {
        size_t cmd_len = strlen(generic_at_args.command->sval[0]);
        s_common_in_str[cmd_len] = '\r';
        s_common_in_str[cmd_len + 1] = '\0';
    }

    if (generic_at_args.timeout->count > 0) {
        timeout = generic_at_args.timeout->ival[0];
    }

    if (generic_at_args.pattern->count > 0) {
        strncpy(s_common_out_str, generic_at_args.pattern->sval[0], sizeof(s_common_out_str));
        handle_line = handle_line_pattern;
    }

    if (esp_modem_dce_generic_command(s_dce, s_common_in_str, timeout, handle_line, s_common_out_str) == ESP_OK) {
        return 0;
    }

    return 1;
}

static int do_modem_lifecycle(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&modem_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, modem_args.end, argv[0]);
        return 1;
    }
    if (modem_args.param->count > 0) {
        esp_err_t err = ESP_FAIL;
        if (strstr(modem_args.param->sval[0], "PPP")) {
            err = esp_modem_start_ppp(s_dce->dte);
        } else if (strstr(modem_args.param->sval[0], "CMD")) {
            err = esp_modem_stop_ppp(s_dce->dte);
        } else {
            return 1;
        }
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "set_working_mode %s succeeded", at_args.param->sval[0]);
        } else {
            ESP_LOGI(TAG, "set_working_mode %s failed with %d", at_args.param->sval[0], err);
        }
        return 0;
    }
    return 1;
}

static void register_dce(void)
{
    at_args.command = arg_str1(NULL, NULL, "<command>", "Symbolic name of DCE command");
    at_args.param_int = arg_int0("i", "int", "<num>", "Input parameter of integer type");
    at_args.param_str = arg_str0("s", "str", "<str>", "Input parameter of string type");
    at_args.param_pdp = arg_str0("p", "pdp", "<pdp>", "Comma separated string with PDP context");
    at_args.param_bool = arg_str0("b", "bool", "<true/false>", "Input parameter of bool type");
    at_args.param = arg_str0(NULL, NULL, "<param>", "Default input argument treated as string");
    at_args.result = arg_str0("o", "out", "<type>", "Type of output parameter");
    at_args.end = arg_end(1);
    const esp_console_cmd_t at_cmd = {
            .command = "dce",
            .help = "send symbolic command to the modem",
            .hint = NULL,
            .func = &do_dce,
            .argtable = &at_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&at_cmd));
}
static void register_at_command(void)
{
    generic_at_args.command = arg_str1(NULL, NULL, "<command>", "AT command to send to the modem");
    generic_at_args.timeout = arg_int0("t", "timeout", "<timeout>", "command timeout");
    generic_at_args.pattern = arg_str0("p", "pattern", "<pattern>", "command response to wait for");
    generic_at_args.no_cr = arg_litn("n", "no-cr", 0, 1, "not add trailing CR to the command");
    generic_at_args.end = arg_end(1);

    const esp_console_cmd_t at_command = {
            .command = "at",
            .help = "send generic AT command to the modem",
            .hint = NULL,
            .func = &do_at_command,
            .argtable = &generic_at_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&at_command));
}

static void register_modem_lifecycle(void)
{
    modem_args.param = arg_str1(NULL, NULL, "<mode>", "PPP or CMD");
    modem_args.end = arg_end(1);
    const esp_console_cmd_t modem_cmd = {
            .command = "modem",
            .help = "set modem mode",
            .hint = NULL,
            .func = &do_modem_lifecycle,
            .argtable = &modem_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&modem_cmd));
}

static esp_console_repl_t *s_repl = NULL;


void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // init the DTE
    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    dte_config.event_task_stack_size = 4096;
    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG("internet");
    dce_config.populate_command_list = true;
    esp_netif_config_t ppp_netif_config = ESP_NETIF_DEFAULT_PPP();

    esp_modem_dte_t *dte = esp_modem_dte_new(&dte_config);
    s_dce = esp_modem_dce_new(&dce_config);
    assert(s_dce != NULL);

    esp_netif_t *esp_netif = esp_netif_new(&ppp_netif_config);
    assert(esp_netif);

    ESP_ERROR_CHECK(esp_modem_default_attach(dte, s_dce, esp_netif));

    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    // init console REPL environment
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &s_repl));

    register_dce();
    register_at_command();
    register_modem_lifecycle();
    modem_console_register_http();
    modem_console_register_ping();

    // start console REPL
    ESP_ERROR_CHECK(esp_console_start_repl(s_repl));
}
