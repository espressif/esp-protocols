/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "eppp_link.h"
#include "eppp_transport.h"
#include "eppp_transport_spi.h"
#include "driver/spi_master.h"
#include "driver/spi_slave.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_rom_crc.h"

#define TAG "eppp_spi"

#define MAX_PAYLOAD 1500
#define MIN_TRIGGER_US 20
#define PPP_SOF 0x7E
#define SPI_HEADER_MAGIC PPP_SOF
#define SPI_ALIGN(size) (((size) + 3U) & ~(3U))
#define TRANSFER_SIZE SPI_ALIGN((MAX_PAYLOAD + 6))
#define NEXT_TRANSACTION_SIZE(a,b) (((a)>(b))?(a):(b)) /* next transaction: whichever is bigger */

struct packet {
    size_t len;
    uint8_t *data;
    int channel;
};

struct header {
    uint8_t magic;
    uint8_t channel;
    uint16_t size;
    uint16_t next_size;
    uint16_t check;
} __attribute__((packed));

enum blocked_status {
    NONE,
    MASTER_BLOCKED,
    MASTER_WANTS_READ,
    SLAVE_BLOCKED,
    SLAVE_WANTS_WRITE,
};

struct eppp_spi {
    struct eppp_handle parent;
    bool is_master;
    QueueHandle_t out_queue;
    QueueHandle_t ready_semaphore;
    spi_device_handle_t spi_device;
    spi_host_device_t spi_host;
    int gpio_intr;
    uint16_t next_size;
    uint16_t transaction_size;
    struct packet outbound;
    enum blocked_status blocked;
    uint32_t slave_last_edge;
    esp_timer_handle_t timer;
};

static esp_err_t transmit_generic(struct eppp_spi *handle, int channel, void *buffer, size_t len)
{

    struct packet buf = { .channel = channel };
    uint8_t *current_buffer = buffer;
    size_t remaining = len;
    do {    // TODO(IDF-9194): Refactor this loop to allocate only once and perform
        //       fragmentation after receiving from the queue (applicable only if MTU > MAX_PAYLOAD)
        size_t batch = remaining > MAX_PAYLOAD ? MAX_PAYLOAD : remaining;
        buf.data = malloc(batch);
        if (buf.data == NULL) {
            ESP_LOGE(TAG, "Failed to allocate packet");
            return ESP_ERR_NO_MEM;
        }
        buf.len = batch;
        remaining -= batch;
        memcpy(buf.data, current_buffer, batch);
        current_buffer += batch;
        BaseType_t ret = xQueueSend(handle->out_queue, &buf, 0);
        if (ret != pdTRUE) {
            ESP_LOGE(TAG, "Failed to queue packet to slave!");
            return ESP_ERR_NO_MEM;
        }
    } while (remaining > 0);

    if (!handle->is_master && handle->blocked == SLAVE_BLOCKED) {
        uint32_t now = esp_timer_get_time();
        uint32_t diff = now - handle->slave_last_edge;
        if (diff < MIN_TRIGGER_US) {
            esp_rom_delay_us(MIN_TRIGGER_US - diff);
        }
        gpio_set_level(handle->gpio_intr, 0);
    }
    return ESP_OK;
}

static esp_err_t transmit(void *h, void *buffer, size_t len)
{
    struct eppp_handle *handle = h;
    struct eppp_spi *spi_handle = __containerof(handle, struct eppp_spi, parent);;
    return transmit_generic(spi_handle, 0, buffer, len);
}

#ifdef CONFIG_EPPP_LINK_CHANNELS_SUPPORT
static esp_err_t transmit_channel(esp_netif_t *netif, int channel, void *buffer, size_t len)
{
    struct eppp_handle *handle = esp_netif_get_io_driver(netif);
    struct eppp_spi *spi_handle = __containerof(handle, struct eppp_spi, parent);;
    return transmit_generic(spi_handle, channel, buffer, len);
}
#endif

static void IRAM_ATTR timer_callback(void *arg)
{
    struct eppp_spi *h = arg;
    if (h->blocked == SLAVE_WANTS_WRITE) {
        gpio_set_level(h->gpio_intr, 0);
    }
}

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    static uint32_t s_last_time;
    uint32_t now = esp_timer_get_time();
    uint32_t diff = now - s_last_time;
    if (diff < MIN_TRIGGER_US) { // debounce
        return;
    }
    s_last_time = now;
    struct eppp_spi *h = arg;
    BaseType_t yield = false;

    // Positive edge means SPI slave prepared the data
    if (gpio_get_level(h->gpio_intr) == 1) {
        xSemaphoreGiveFromISR(h->ready_semaphore, &yield);
        if (yield) {
            portYIELD_FROM_ISR();
        }
        return;
    }

    // Negative edge (when master blocked) means that slave wants to transmit
    if (h->blocked == MASTER_BLOCKED) {
        struct packet buf = { .data = NULL, .len = -1 };
        xQueueSendFromISR(h->out_queue, &buf, &yield);
        if (yield) {
            portYIELD_FROM_ISR();
        }
    }
}

static esp_err_t deinit_master(struct eppp_spi *h)
{
    ESP_RETURN_ON_ERROR(spi_bus_remove_device(h->spi_device), TAG, "Failed to remove SPI bus");
    ESP_RETURN_ON_ERROR(spi_bus_free(h->spi_host), TAG, "Failed to free SPI bus");
    return ESP_OK;
}

static esp_err_t init_master(struct eppp_config_spi_s *config, struct eppp_spi *h)
{
    esp_err_t ret = ESP_OK;
    h->spi_host = config->host;
    h->gpio_intr = config->intr;
    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = config->mosi;
    bus_cfg.miso_io_num = config->miso;
    bus_cfg.sclk_io_num = config->sclk;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = TRANSFER_SIZE;
    bus_cfg.flags = 0;
    bus_cfg.intr_flags = 0;

    // TODO(IDF-13351): Init and deinit SPI bus separately (per Kconfig?)
    ESP_RETURN_ON_ERROR(spi_bus_initialize(config->host, &bus_cfg, SPI_DMA_CH_AUTO), TAG, "Failed to init SPI bus");

    spi_device_interface_config_t dev_cfg = {};
    dev_cfg.clock_speed_hz = config->freq;
    dev_cfg.mode = 0;
    dev_cfg.spics_io_num = config->cs;
    dev_cfg.cs_ena_pretrans = config->cs_ena_pretrans;
    dev_cfg.cs_ena_posttrans = config->cs_ena_posttrans;
    dev_cfg.duty_cycle_pos = 128;
    dev_cfg.input_delay_ns = config->input_delay_ns;
    dev_cfg.pre_cb = NULL;
    dev_cfg.post_cb = NULL;
    dev_cfg.queue_size = 3;

    ESP_GOTO_ON_ERROR(spi_bus_add_device(config->host, &dev_cfg, &h->spi_device), err, TAG, "Failed to add SPI device");

    //GPIO config for the handshake line.
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_ANYEDGE,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
        .pin_bit_mask = BIT64(config->intr),
    };

    ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err_dev, TAG, "Failed to config interrupt GPIO");
    ret = gpio_install_isr_service(0);
    ESP_GOTO_ON_FALSE(ret == ESP_OK || ret == ESP_ERR_INVALID_STATE /* In case the GPIO ISR already installed */,
                      ret, err_dev, TAG, "Failed to install GPIO ISR");
    ESP_GOTO_ON_ERROR(gpio_set_intr_type(config->intr, GPIO_INTR_ANYEDGE), err_dev, TAG, "Failed to set ISR type");
    ESP_GOTO_ON_ERROR(gpio_isr_handler_add(config->intr, gpio_isr_handler, h), err_dev, TAG, "Failed to add ISR handler");
    return ESP_OK;
err_dev:
    spi_bus_remove_device(h->spi_device);
err:
    spi_bus_free(config->host);
    return ret;
}

static void post_setup(spi_slave_transaction_t *trans)
{
    struct eppp_spi *h = trans->user;
    h->slave_last_edge = esp_timer_get_time();
    gpio_set_level(h->gpio_intr, 1);
    if (h->transaction_size == 0) { // If no transaction planned:
        if (h->outbound.len == 0) { // we're blocked if we don't have any data
            h->blocked = SLAVE_BLOCKED;
        } else {
            h->blocked = SLAVE_WANTS_WRITE; // we notify the master that we want to write
            esp_timer_start_once(h->timer, MIN_TRIGGER_US);
        }
    }
}

static void post_transaction(spi_slave_transaction_t *transaction)
{
    struct eppp_spi *h = transaction->user;
    h->blocked = NONE;
    gpio_set_level(h->gpio_intr, 0);
}

static esp_err_t deinit_slave(struct eppp_spi *h)
{
    ESP_RETURN_ON_ERROR(spi_slave_free(h->spi_host), TAG, "Failed to free SPI slave host");
    ESP_RETURN_ON_ERROR(spi_bus_remove_device(h->spi_device), TAG, "Failed to remove SPI device");
    ESP_RETURN_ON_ERROR(spi_bus_free(h->spi_host), TAG, "Failed to free SPI bus");
    return ESP_OK;
}

static esp_err_t init_slave(struct eppp_config_spi_s *config, struct eppp_spi *h)
{
    h->spi_host = config->host;
    h->gpio_intr = config->intr;
    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = config->mosi;
    bus_cfg.miso_io_num = config->miso;
    bus_cfg.sclk_io_num = config->sclk;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.flags = 0;
    bus_cfg.intr_flags = 0;

    //Configuration for the SPI slave interface
    spi_slave_interface_config_t slvcfg = {
        .mode = 0,
        .spics_io_num = config->cs,
        .queue_size = 3,
        .flags = 0,
        .post_setup_cb = post_setup,
        .post_trans_cb = post_transaction,
    };

    //Configuration for the handshake line
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = BIT64(config->intr),
    };

    gpio_config(&io_conf);
    gpio_set_pull_mode(config->mosi, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(config->sclk, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(config->cs, GPIO_PULLUP_ONLY);

    //Initialize SPI slave interface
    if (spi_slave_initialize(config->host, &bus_cfg, &slvcfg, SPI_DMA_CH_AUTO) != ESP_OK) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

typedef esp_err_t (*perform_transaction_t)(struct eppp_spi *h, size_t len, const void *tx_buffer, void *rx_buffer);

static esp_err_t perform_transaction_master(struct eppp_spi *h, size_t len, const void *tx_buffer, void *rx_buffer)
{
    spi_transaction_t t = {};
    t.length = len * 8;
    t.tx_buffer = tx_buffer;
    t.rx_buffer = rx_buffer;
    return spi_device_transmit(h->spi_device, &t);
}

static esp_err_t perform_transaction_slave(struct eppp_spi *h, size_t len, const void *tx_buffer, void *rx_buffer)
{
    spi_slave_transaction_t t = {};
    t.user = h;
    t.length = len * 8;
    t.tx_buffer = tx_buffer;
    t.rx_buffer = rx_buffer;
    return spi_slave_transmit(h->spi_host, &t, portMAX_DELAY);
}

esp_err_t eppp_perform(esp_netif_t *netif)
{
    static WORD_ALIGNED_ATTR uint8_t out_buf[TRANSFER_SIZE] = {};
    static WORD_ALIGNED_ATTR uint8_t in_buf[TRANSFER_SIZE] = {};

    struct eppp_handle *handle = esp_netif_get_io_driver(netif);
    struct eppp_spi *h = __containerof(handle, struct eppp_spi, parent);

    // Perform transaction for master and slave
    const perform_transaction_t perform_transaction = h->is_master ? perform_transaction_master : perform_transaction_slave;

    if (h->parent.stop) {
        return ESP_ERR_TIMEOUT;
    }

    BaseType_t tx_queue_stat;
    bool allow_test_tx = false;
    uint16_t next_tx_size = 0;
    if (h->is_master) {
        // SPI MASTER only code
        if (xSemaphoreTake(h->ready_semaphore, pdMS_TO_TICKS(1000)) != pdTRUE) {
            // slave might not be ready, but maybe we just missed an interrupt
            allow_test_tx = true;
        }
        if (h->outbound.len == 0 && h->transaction_size == 0 && h->blocked == NONE) {
            h->blocked = MASTER_BLOCKED;
            xQueueReceive(h->out_queue, &h->outbound, portMAX_DELAY);
            h->blocked = NONE;
            if (h->outbound.len == -1) {
                h->outbound.len = 0;
                h->blocked = MASTER_WANTS_READ;
            }
        } else if (h->blocked == MASTER_WANTS_READ) {
            h->blocked = NONE;
        }
    }
    struct header *head = (void *)out_buf;
    if (h->outbound.len <= h->transaction_size && allow_test_tx == false) {
        // sending outbound
        head->size = h->outbound.len;
        head->channel = h->outbound.channel;
        if (h->outbound.len > 0) {
            memcpy(out_buf + sizeof(struct header), h->outbound.data, h->outbound.len);
            free(h->outbound.data);
            ESP_LOG_BUFFER_HEXDUMP(TAG, out_buf + sizeof(struct header), head->size, ESP_LOG_VERBOSE);
            h->outbound.data = NULL;
            h->outbound.len = 0;
        }
        do {
            tx_queue_stat = xQueueReceive(h->out_queue, &h->outbound, 0);
        } while (tx_queue_stat == pdTRUE && h->outbound.len == -1);
        if (h->outbound.len == -1) { // used as a signal only, no actual data
            h->outbound.len = 0;
        }
    } else {
        // outbound is bigger, need to transmit in another transaction (keep this empty)
        head->size = 0;
        head->channel = 0;
    }
    next_tx_size = head->next_size = h->outbound.len;
    head->magic = SPI_HEADER_MAGIC;
    head->check = esp_rom_crc16_le(0, out_buf, sizeof(struct header) - sizeof(uint16_t));
    esp_err_t ret = perform_transaction(h, sizeof(struct header) + h->transaction_size, out_buf, in_buf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_device_transmit failed");
        h->transaction_size = 0; // need to start with HEADER only transaction
        return ESP_FAIL;
    }
    head = (void *)in_buf;
    uint16_t check = esp_rom_crc16_le(0, in_buf, sizeof(struct header) - sizeof(uint16_t));
    if (check != head->check || head->magic != SPI_HEADER_MAGIC || head->channel > NR_OF_CHANNELS) {
        h->transaction_size = 0; // need to start with HEADER only transaction
        if (allow_test_tx) {
            return ESP_OK;
        }
        ESP_LOGE(TAG, "Wrong checksum, magic, or channel: %x %x %x", check, head->magic, head->channel);
        return ESP_FAIL;
    }
    if (head->size > 0) {
        ESP_LOG_BUFFER_HEXDUMP(TAG, in_buf + sizeof(struct header), head->size, ESP_LOG_VERBOSE);
        if (head->channel == 0) {
            esp_netif_receive(netif, in_buf + sizeof(struct header), head->size, NULL);
        } else {
#if defined(CONFIG_EPPP_LINK_CHANNELS_SUPPORT)
            if (h->parent.channel_rx) {
                h->parent.channel_rx(netif, head->channel, in_buf + sizeof(struct header), head->size);
            }
#endif
        }
    }
    h->transaction_size = NEXT_TRANSACTION_SIZE(next_tx_size, head->next_size);
    return ESP_OK;
}


static esp_err_t init_driver(struct eppp_spi *h, struct eppp_config_spi_s *config)
{
    if (config->is_master) {
        return init_master(config, h);
    }
    return init_slave(config, h);
}

static esp_err_t post_attach(esp_netif_t *esp_netif, void *args)
{
    eppp_transport_handle_t h = (eppp_transport_handle_t)args;
    ESP_RETURN_ON_FALSE(h, ESP_ERR_INVALID_ARG, TAG, "Transport handle cannot be null");
    h->base.netif = esp_netif;

    esp_netif_driver_ifconfig_t driver_ifconfig = {
        .handle =  h,
        .transmit = transmit,
    };

    ESP_RETURN_ON_ERROR(esp_netif_set_driver_config(esp_netif, &driver_ifconfig), TAG, "Failed to set driver config");
    ESP_LOGI(TAG, "EPPP SPI transport attached to EPPP netif %s", esp_netif_get_desc(esp_netif));
    return ESP_OK;
}


eppp_transport_handle_t eppp_spi_init(struct eppp_config_spi_s *config)
{
    __attribute__((unused)) esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(config, NULL, TAG, "Config cannot be null");
    struct eppp_spi *h = calloc(1, sizeof(struct eppp_spi));
    ESP_RETURN_ON_FALSE(h, NULL, TAG, "Failed to allocate eppp_handle");
#ifdef CONFIG_EPPP_LINK_CHANNELS_SUPPORT
    h->parent.channel_tx = transmit_channel;
#endif
    h->is_master = config->is_master;
    h->parent.base.post_attach = post_attach;
    h->out_queue = xQueueCreate(CONFIG_EPPP_LINK_PACKET_QUEUE_SIZE, sizeof(struct packet));
    ESP_GOTO_ON_FALSE(h->out_queue, ESP_FAIL, err, TAG, "Failed to create the packet queue");
    if (h->is_master) {
        ESP_GOTO_ON_FALSE(h->ready_semaphore = xSemaphoreCreateBinary(), ESP_FAIL, err, TAG, "Failed to create the semaphore");
    }
    h->transaction_size = 0;
    h->outbound.data = NULL;
    h->outbound.len = 0;
    if (!h->is_master) {
        esp_timer_create_args_t args = {
            .callback = &timer_callback,
            .arg = h,
            .name = "spi_slave_tmr"
        };
        ESP_GOTO_ON_ERROR(esp_timer_create(&args, &h->timer), err, TAG, "Failed to create timer");
    }
    ESP_GOTO_ON_ERROR(init_driver(h, config), err, TAG, "Failed to init SPI driver");
    return &h->parent;
err:
    if (h->out_queue) {
        vQueueDelete(h->out_queue);
    }
    if (h->ready_semaphore) {
        vSemaphoreDelete(h->ready_semaphore);
    }
    free(h);
    return NULL;
}
void eppp_spi_deinit(eppp_transport_handle_t handle)
{
    struct eppp_spi *h = __containerof(handle, struct eppp_spi, parent);;
    if (h->is_master) {
        deinit_master(h);
    } else {
        deinit_slave(h);
    }
    struct packet buf = { };
    while (xQueueReceive(h->out_queue, &buf, 0) == pdTRUE) {
        if (buf.len > 0) {
            free(buf.data);
        }
    }
    vQueueDelete(h->out_queue);
    if (h->is_master) {
        vSemaphoreDelete(h->ready_semaphore);
    }
    free(h);
}
