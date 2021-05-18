/*  softAP to PPPoS Example (network_dce)

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>
#include "esp_netif.h"
#include "esp_modem_api.h"


static esp_modem_dce_t *dce = NULL;


esp_err_t modem_init_network(esp_netif_t *netif)
{
    // setup the DCE
    esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(CONFIG_EXAMPLE_MODEM_PPP_APN);
    dce = esp_modem_new(&dte_config, &dce_config, netif);
    if (!dce) {
        return ESP_FAIL;
    }

    // configure the PIN
    bool pin_ok = false;
    if (esp_modem_read_pin(dce, &pin_ok) == ESP_OK && pin_ok == false) {
        if (esp_modem_set_pin(dce, CONFIG_EXAMPLE_SIM_PIN) == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        } else {
            abort();
        }
    }
    return ESP_OK;
}

void modem_deinit_network(void)
{
    if (dce) {
        esp_modem_destroy(dce);
        dce = NULL;
    }
}

void modem_start_network()
{
    esp_modem_set_mode(dce, ESP_MODEM_MODE_DATA);
}

void modem_stop_network()
{
    esp_modem_set_mode(dce, ESP_MODEM_MODE_COMMAND);
}
