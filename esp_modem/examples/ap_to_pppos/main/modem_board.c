/*  softAP to PPPoS Example (modem_board)

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_modem_recov_helper.h"
#include "esp_modem_dce.h"
#include "esp_modem_dce_common_commands.h"

#define ESP_MODEM_EXAMPLE_CHECK(a, str, goto_tag, ...)                                \
    do                                                                                \
    {                                                                                 \
        if (!(a))                                                                     \
        {                                                                             \
            ESP_LOGE(TAG, "%s(%d): " str, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
            goto goto_tag;                                                            \
        }                                                                             \
    } while (0)


static const char *TAG = "sim7600_board";

typedef struct {
    esp_modem_dce_t parent;
    esp_modem_recov_gpio_t *power_pin;
    esp_modem_recov_gpio_t *reset_pin;
    esp_err_t (*reset)(esp_modem_dce_t *dce);
    esp_err_t (*power_down)(esp_modem_dce_t *dce);
    esp_modem_recov_resend_t *re_sync;
    esp_modem_recov_resend_t *re_store_profile;
} sim7600_board_t;

esp_err_t sim7600_board_handle_powerup(esp_modem_dce_t *dce, const char *line)
{
    if (strstr(line, "PB DONE")) {
        ESP_LOGI(TAG, "Board ready after hard reset/power-cycle");
    }
    return ESP_OK;
}

esp_err_t sim7600_board_deinit(esp_modem_dce_t *dce)
{
    sim7600_board_t *board = __containerof(dce, sim7600_board_t, parent);
    board->power_pin->destroy(board->power_pin);
    board->power_pin->destroy(board->reset_pin);
    esp_err_t err = esp_modem_command_list_deinit(&board->parent);
    if (err == ESP_OK) {
        free(dce);
    }
    return err;
}

esp_err_t sim7600_board_reset(esp_modem_dce_t *dce)
{
    sim7600_board_t *board = __containerof(dce, sim7600_board_t, parent);
    ESP_LOGI(TAG, "sim7600_board_reset!");
    dce->handle_line = sim7600_board_handle_powerup;
    board->power_pin->pulse(board->reset_pin);
    return ESP_OK;
}

esp_err_t sim7600_board_power_up(esp_modem_dce_t *dce)
{
    sim7600_board_t *board = __containerof(dce, sim7600_board_t, parent);
    ESP_LOGI(TAG, "sim7600_board_power_up!");
    dce->handle_line = sim7600_board_handle_powerup;
    board->power_pin->pulse(board->power_pin);
    return ESP_OK;
}


esp_err_t sim7600_board_power_down(esp_modem_dce_t *dce)
{
    sim7600_board_t *board = __containerof(dce, sim7600_board_t, parent);
    ESP_LOGI(TAG, "sim7600_board_power_down!");
    /* power down sequence (typical values for SIM7600 Toff=min2.5s, Toff-status=26s) */
    dce->handle_line = sim7600_board_handle_powerup;
    board->power_pin->pulse_special(board->power_pin, 3000, 26000);
    return ESP_OK;
}

static esp_err_t my_recov(esp_modem_recov_resend_t *retry_cmd, esp_err_t err, int timeouts, int errors)
{
    esp_modem_dce_t *dce = retry_cmd->dce;
    ESP_LOGI(TAG, "Current timeouts: %d and errors: %d", timeouts, errors);
    if (err == ESP_ERR_TIMEOUT) {
        if (timeouts < 2) {
            // first timeout, try to exit data mode and sync again
            dce->set_command_mode(dce, NULL, NULL);
            esp_modem_dce_sync(dce, NULL, NULL);
        } else if (timeouts < 3) {
            // try to reset with GPIO if resend didn't help
            sim7600_board_t *board = __containerof(dce, sim7600_board_t, parent);
            board->reset(dce);
        } else {
            // otherwise power-cycle the board
            sim7600_board_t *board = __containerof(dce, sim7600_board_t, parent);
            board->power_down(dce);
            esp_modem_dce_sync(dce, NULL, NULL);
        }
    } else {
        // check if a PIN needs to be supplied in case of a failure
        bool ready = false;
        esp_modem_dce_read_pin(dce, NULL, &ready);
        if (!ready) {
            esp_modem_dce_set_pin(dce, "1234", NULL);
        }
        vTaskDelay(1000 / portTICK_RATE_MS);
        esp_modem_dce_read_pin(dce, NULL, &ready);
        if (!ready) {
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

static DEFINE_RETRY_CMD(re_sync_fn, re_sync, sim7600_board_t)

static DEFINE_RETRY_CMD(re_store_profile_fn, re_store_profile, sim7600_board_t)

esp_err_t sim7600_board_start_up(esp_modem_dce_t *dce)
{
//    sim7600_board_t *board = __containerof(dce, sim7600_board_t, parent);
    ESP_MODEM_EXAMPLE_CHECK(re_sync_fn(dce, NULL, NULL) == ESP_OK, "sending sync failed", err);
    ESP_MODEM_EXAMPLE_CHECK(dce->set_echo(dce, (void*)false, NULL) == ESP_OK, "set_echo failed", err);
    ESP_MODEM_EXAMPLE_CHECK(dce->set_flow_ctrl(dce, (void*)ESP_MODEM_FLOW_CONTROL_NONE, NULL) == ESP_OK, "set_flow_ctrl failed", err);
    ESP_MODEM_EXAMPLE_CHECK(dce->store_profile(dce, NULL, NULL) == ESP_OK, "store_profile failed", err);
    return ESP_OK;
err:
    return ESP_FAIL;

}

esp_modem_dce_t *sim7600_board_create(esp_modem_dce_config_t *config)
{
    sim7600_board_t *board = calloc(1, sizeof(sim7600_board_t));
    ESP_MODEM_EXAMPLE_CHECK(board, "failed to allocate board-sim7600 object", err);
    ESP_MODEM_EXAMPLE_CHECK(esp_modem_dce_init(&board->parent, config) == ESP_OK, "Failed to init sim7600", err);
    /* power on sequence (typical values for SIM7600 Ton=500ms, Ton-status=16s) */
    board->power_pin = esp_modem_recov_gpio_new( /*gpio_num*/ 12, /*inactive_level*/ 1, /*active_width*/
                                                              500, /*inactive_width*/ 16000);
    /* reset sequence (typical values for SIM7600 Treset=200ms, wait 10s after reset */
    board->reset_pin = esp_modem_recov_gpio_new( /*gpio_num*/ 13, /*inactive_level*/ 1, /*active_width*/
                                                              200, /*inactive_width*/ 10000);
    board->parent.deinit = sim7600_board_deinit;
    board->reset = sim7600_board_reset;
    board->power_down = sim7600_board_power_down;
    board->re_sync = esp_modem_recov_resend_new(&board->parent, board->parent.sync, my_recov, 5, 1);
    board->parent.start_up = sim7600_board_start_up;
    board->re_store_profile = esp_modem_recov_resend_new(&board->parent, board->parent.store_profile, my_recov, 2, 3);
    board->parent.store_profile = re_store_profile_fn;

    return &board->parent;
err:
    return NULL;
}