/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "eppp_link.h"
#include "esp_log.h"
#include "esp_check.h"
#include "console_ping.h"
#include "esp_eth_driver.h"
#include "driver/gpio.h"
#include "esp_eth_phy_dummy.h"

#ifdef CONFIG_EXAMPLE_NODE_SERVER
#define EPPP_CONFIG() EPPP_DEFAULT_SERVER_CONFIG()
#define EPPP_ROLE EPPP_SERVER
#define EPPP_RMII_CLK_SINK
#else
#define EPPP_CONFIG() EPPP_DEFAULT_CLIENT_CONFIG()
#define EPPP_ROLE EPPP_CLIENT
#define EPPP_RMII_CLK_SOURCE
#endif

void register_iperf(void);

static const char *TAG = "eppp_emac2emac";


static esp_eth_mac_t *s_mac = NULL;
static esp_eth_phy_t *s_phy = NULL;

#ifdef EPPP_RMII_CLK_SOURCE
IRAM_ATTR static void gpio_isr_handler(void *arg)
{
    BaseType_t high_task_wakeup = pdFALSE;
    TaskHandle_t task_handle = (TaskHandle_t)arg;

    vTaskNotifyGiveFromISR(task_handle, &high_task_wakeup);
    if (high_task_wakeup != pdFALSE) {
        portYIELD_FROM_ISR();
    }
}
#else
#define STARTUP_DELAY_MS 500
#endif

esp_err_t eppp_transport_ethernet_init(esp_eth_handle_t *handle[])
{
    *handle = malloc(sizeof(esp_eth_handle_t));
    ESP_RETURN_ON_FALSE(handle, ESP_ERR_NO_MEM, TAG, "Our of memory");

#ifdef EPPP_RMII_CLK_SOURCE
    esp_rom_gpio_pad_select_gpio(EMAC_CLK_OUT_180_GPIO);
    gpio_set_pull_mode(EMAC_CLK_OUT_180_GPIO, GPIO_FLOATING); // to not affect GPIO0 (so the Sink Device could be flashed)
    gpio_install_isr_service(0);
    gpio_config_t gpio_source_cfg = {
        .pin_bit_mask = (1ULL << CONFIG_EXAMPLE_RMII_CLK_READY_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    gpio_config(&gpio_source_cfg);
    TaskHandle_t task_handle = xTaskGetHandle(pcTaskGetName(NULL));
    gpio_isr_handler_add(CONFIG_EXAMPLE_RMII_CLK_READY_GPIO, gpio_isr_handler, task_handle);
    ESP_LOGW(TAG, "waiting for RMII CLK sink device interrupt");
    ESP_LOGW(TAG, "if RMII CLK sink device is already running, reset it by `EN` button");
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (gpio_get_level(CONFIG_EXAMPLE_RMII_CLK_READY_GPIO) == 1) {
            break;
        }
    }
    ESP_LOGI(TAG, "starting Ethernet initialization");
#else
    gpio_config_t gpio_sink_cfg = {
        .pin_bit_mask = (1ULL << CONFIG_EXAMPLE_RMII_CLK_READY_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&gpio_sink_cfg);
    gpio_set_level(CONFIG_EXAMPLE_RMII_CLK_READY_GPIO, 0);
    vTaskDelay(pdMS_TO_TICKS(STARTUP_DELAY_MS));
    gpio_set_level(CONFIG_EXAMPLE_RMII_CLK_READY_GPIO, 1);
#endif // EPPP_RMII_CLK_SOURCE

    // --- Initialize Ethernet driver ---
    // Init common MAC and PHY configs to default
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();

    // Update PHY config based on board specific configuration
    phy_config.reset_gpio_num = -1; // no HW reset

    // Init vendor specific MAC config to default
    eth_esp32_emac_config_t esp32_emac_config = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    // Update vendor specific MAC config based on board configuration
    // No SMI, speed/duplex must be statically configured the same in both devices
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
    esp32_emac_config.smi_gpio.mdc_num = -1;
    esp32_emac_config.smi_gpio.mdio_num = -1;
#else
    esp32_emac_config.smi_mdc_gpio_num = -1;
    esp32_emac_config.smi_mdio_gpio_num = -1;
#endif
#ifdef EPPP_RMII_CLK_SOURCE
    esp32_emac_config.clock_config.rmii.clock_mode = EMAC_CLK_OUT;
    esp32_emac_config.clock_config.rmii.clock_gpio = EMAC_CLK_OUT_180_GPIO;
#else
    esp32_emac_config.clock_config.rmii.clock_mode = EMAC_CLK_EXT_IN;
    esp32_emac_config.clock_config.rmii.clock_gpio = EMAC_CLK_IN_GPIO;
#endif // EPPP_RMII_CLK_SOURCE

    // Create new ESP32 Ethernet MAC instance
    s_mac = esp_eth_mac_new_esp32(&esp32_emac_config, &mac_config);
    // Create dummy PHY instance
    s_phy = esp_eth_phy_new_dummy(&phy_config);

    // Init Ethernet driver to default and install it
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(s_mac, s_phy);
#ifdef EPPP_RMII_CLK_SINK
    // REF RMII CLK sink device performs multiple EMAC init attempts since RMII CLK source device may not be ready yet
    int i;
    for (i = 1; i <= 5; i++) {
        ESP_LOGI(TAG, "Ethernet driver install attempt: %i", i);
        if (esp_eth_driver_install(&config, *handle) == ESP_OK) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    ESP_RETURN_ON_FALSE(i <= 5, ESP_FAIL, TAG, "Ethernet driver install failed");
#else
    ESP_RETURN_ON_ERROR(esp_eth_driver_install(&config, *handle), TAG, "Ethernet driver install failed");
#endif // EPPP_RMII_CLK_SINK
    return ESP_OK;

}

void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Sets up the default EPPP-connection
     */
    eppp_config_t config = EPPP_CONFIG();
    config.transport = EPPP_TRANSPORT_ETHERNET;
    esp_netif_t *eppp_netif = eppp_open(EPPP_ROLE, &config, portMAX_DELAY);
    if (eppp_netif == NULL) {
        ESP_LOGE(TAG, "Failed to connect");
        return ;
    }
    // Initialize console REPL
    ESP_ERROR_CHECK(console_cmd_init());

    register_iperf();

    printf("\n =======================================================\n");
    printf(" |       Steps to Test EPPP-emac2emca bandwidth        |\n");
    printf(" |                                                     |\n");
    printf(" |  1. Wait for the ESP32 to get an IP                 |\n");
    printf(" |  2. Server: 'iperf -u -s -i 3' (on host)            |\n");
    printf(" |  3. Client: 'iperf -u -c SERVER_IP -t 60 -i 3'      |\n");
    printf(" |                                                     |\n");
    printf(" =======================================================\n\n");

    // using also ping command to check basic network connectivity
    ESP_ERROR_CHECK(console_cmd_ping_register());
    ESP_ERROR_CHECK(console_cmd_start());

    // handle GPIO0 workaround for ESP32
#ifdef CONFIG_EXAMPLE_NODE_SERVER
    // Wait indefinitely or reset when "RMII CLK Sink Device" resets
    // We reset the "RMII CLK Source Device" to ensure there is no CLK at GPIO0 of the
    // "RMII CLK Sink Device" during startup
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (gpio_get_level(CONFIG_EXAMPLE_RMII_CLK_READY_GPIO) == 0) {
            break;
        }
    }
    ESP_LOGW(TAG, "RMII CLK Sink device reset, I'm going to reset too!");
    esp_restart();
#endif
}
