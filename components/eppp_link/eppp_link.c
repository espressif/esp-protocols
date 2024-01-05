/*
 * SPDX-FileCopyrightText: 2019-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string.h>
#include <stdint.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_netif_ppp.h"
#include "eppp_link.h"

#if CONFIG_EPPP_LINK_DEVICE_SPI
#include "driver/spi_master.h"
#include "driver/spi_slave.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#elif CONFIG_EPPP_LINK_DEVICE_UART
#include "driver/uart.h"
#endif

static const int GOT_IPV4 = BIT0;
static const int CONNECTION_FAILED = BIT1;
#define CONNECT_BITS (GOT_IPV4|CONNECTION_FAILED)

static EventGroupHandle_t s_event_group = NULL;
static const char *TAG = "eppp_link";
static int s_retry_num = 0;
static int s_eppp_netif_count = 0; // used as a suffix for the netif key

struct eppp_handle {
#if CONFIG_EPPP_LINK_DEVICE_SPI
    QueueHandle_t out_queue;
    QueueHandle_t ready_semaphore;
    spi_device_handle_t spi_device;
    spi_host_device_t spi_host;
    int gpio_intr;
#elif CONFIG_EPPP_LINK_DEVICE_UART
    QueueHandle_t uart_event_queue;
    uart_port_t uart_port;
#endif
    esp_netif_t *netif;
    enum eppp_type role;
    bool stop;
    bool exited;
    bool netif_stop;
};

struct packet {
    size_t len;
    uint8_t *data;
};

static esp_err_t transmit(void *h, void *buffer, size_t len)
{
    struct eppp_handle *handle = h;
#if CONFIG_EPPP_LINK_DEVICE_SPI
#define MAX_PAYLOAD 1600
    struct packet buf = { };

    uint8_t *current_buffer = buffer;
    size_t remaining = len;
    do {
        size_t batch = remaining > MAX_PAYLOAD ? MAX_PAYLOAD : remaining;
        buf.data = malloc(batch);
        if (buf.data == NULL) {
            ESP_LOGE(TAG, "Failed to allocate packet");
            return ESP_FAIL;
        }
        buf.len = batch;
        remaining -= batch;
        memcpy(buf.data, current_buffer, batch);
        current_buffer += batch;
        BaseType_t ret = xQueueSend(handle->out_queue, &buf, pdMS_TO_TICKS(10));
        if (ret != pdTRUE) {
            ESP_LOGE(TAG, "Failed to queue packet to slave!");
            return ESP_FAIL;
        }
    } while (remaining > 0);
#elif CONFIG_EPPP_LINK_DEVICE_UART
    ESP_LOG_BUFFER_HEXDUMP("ppp_uart_send", buffer, len, ESP_LOG_VERBOSE);
    uart_write_bytes(handle->uart_port, buffer, len);
#endif
    return ESP_OK;
}

static void netif_deinit(esp_netif_t *netif)
{
    if (netif == NULL) {
        return;
    }
    struct eppp_handle *h = esp_netif_get_io_driver(netif);
    if (h == NULL) {
        return;
    }
#if CONFIG_EPPP_LINK_DEVICE_SPI
    vQueueDelete(h->out_queue);
    if (h->role == EPPP_CLIENT) {
        vSemaphoreDelete(h->ready_semaphore);
    }
#endif
    free(h);
    esp_netif_destroy(netif);
    if (s_eppp_netif_count > 0) {
        s_eppp_netif_count--;
    }
}

static esp_netif_t *netif_init(enum eppp_type role)
{
    if (s_eppp_netif_count > 9) {
        ESP_LOGE(TAG, "Cannot create more than 10 instances");
        return NULL;
    }

    // Create the object first
    struct eppp_handle *h = calloc(1, sizeof(struct eppp_handle));
    if (!h) {
        ESP_LOGE(TAG, "Failed to allocate eppp_handle");
        return NULL;
    }
    h->role = role;
#if CONFIG_EPPP_LINK_DEVICE_SPI
    h->out_queue = xQueueCreate(CONFIG_EPPP_LINK_PACKET_QUEUE_SIZE, sizeof(struct packet));
    if (!h->out_queue) {
        ESP_LOGE(TAG, "Failed to create the packet queue");
        free(h);
        return NULL;
    }
    if (role == EPPP_CLIENT) {
        h->ready_semaphore = xSemaphoreCreateBinary();
        if (!h->ready_semaphore) {
            ESP_LOGE(TAG, "Failed to create the packet queue");
            vQueueDelete(h->out_queue);
            free(h);
            return NULL;
        }
    }
#endif

    esp_netif_driver_ifconfig_t driver_cfg = {
        .handle = h,
        .transmit = transmit,
    };
    const esp_netif_driver_ifconfig_t *ppp_driver_cfg = &driver_cfg;

    esp_netif_inherent_config_t base_netif_cfg = ESP_NETIF_INHERENT_DEFAULT_PPP();
    char if_key[] = "EPPP0"; // netif key needs to be unique
    if_key[sizeof(if_key) - 2 /* 2 = two chars before the terminator */ ] += s_eppp_netif_count++;
    base_netif_cfg.if_key = if_key;
    if (role == EPPP_CLIENT) {
        base_netif_cfg.if_desc = "pppos_client";
    } else {
        base_netif_cfg.if_desc = "pppos_server";
    }
    esp_netif_config_t netif_ppp_config = { .base = &base_netif_cfg,
                                            .driver = ppp_driver_cfg,
                                            .stack = ESP_NETIF_NETSTACK_DEFAULT_PPP
                                          };

    esp_netif_t *netif = esp_netif_new(&netif_ppp_config);
    if (!netif) {
        ESP_LOGE(TAG, "Failed to create esp_netif");
#if CONFIG_EPPP_LINK_DEVICE_SPI
        vQueueDelete(h->out_queue);
#endif
        free(h);
        return NULL;
    }
    return netif;

}

esp_err_t eppp_netif_stop(esp_netif_t *netif, TickType_t stop_timeout)
{
    esp_netif_action_disconnected(netif, 0, 0, 0);
    esp_netif_action_stop(netif, 0, 0, 0);
    struct eppp_handle *h = esp_netif_get_io_driver(netif);
    for (int wait = 0; wait < 100; wait++) {
        vTaskDelay(stop_timeout / 100);
        if (h->netif_stop) {
            break;
        }
    }
    if (!h->netif_stop) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t eppp_netif_start(esp_netif_t *netif)
{
    esp_netif_action_start(netif, 0, 0, 0);
    esp_netif_action_connected(netif, 0, 0, 0);
    return ESP_OK;
}

static int get_netif_num(esp_netif_t *netif)
{
    if (netif == NULL) {
        return -1;
    }
    const char *ifkey = esp_netif_get_ifkey(netif);
    if (strstr(ifkey, "EPPP") == NULL) {
        return -1; // not our netif
    }
    int netif_cnt = ifkey[4] - '0';
    if (netif_cnt < 0 || netif_cnt > 9) {
        ESP_LOGE(TAG, "Unexpected netif key %s", ifkey);
        return -1;
    }
    return  netif_cnt;
}

static void on_ppp_event(void *arg, esp_event_base_t base, int32_t event_id, void *data)
{
    esp_netif_t **netif = data;
    if (base == NETIF_PPP_STATUS && event_id == NETIF_PPP_ERRORUSER) {
        ESP_LOGI(TAG, "Disconnected %d", get_netif_num(*netif));
        struct eppp_handle *h = esp_netif_get_io_driver(*netif);
        h->netif_stop = true;
    }
}

static void on_ip_event(void *arg, esp_event_base_t base, int32_t event_id, void *data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
    esp_netif_t *netif = event->esp_netif;
    int netif_cnt = get_netif_num(netif);
    if (netif_cnt < 0) {
        return;
    }
    if (event_id == IP_EVENT_PPP_GOT_IP) {
        ESP_LOGI(TAG, "Got IPv4 event: Interface \"%s(%s)\" address: " IPSTR, esp_netif_get_desc(netif),
                 esp_netif_get_ifkey(netif), IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_event_group, GOT_IPV4 << (netif_cnt * 2));
    } else if (event_id == IP_EVENT_PPP_LOST_IP) {
        ESP_LOGI(TAG, "Disconnected");
        s_retry_num++;
        if (s_retry_num > CONFIG_EPPP_LINK_CONN_MAX_RETRY) {
            ESP_LOGE(TAG, "PPP Connection failed %d times, stop reconnecting.", s_retry_num);
            xEventGroupSetBits(s_event_group, CONNECTION_FAILED << (netif_cnt * 2));
        } else {
            ESP_LOGI(TAG, "PPP Connection failed %d times, try to reconnect.", s_retry_num);
            eppp_netif_start(netif);
        }
    }
}

#if CONFIG_EPPP_LINK_DEVICE_SPI

#define TRANSFER_SIZE (MAX_PAYLOAD + 4)
#define SHORT_PAYLOAD (48)
#define CONTROL_SIZE (SHORT_PAYLOAD + 4)

#define CONTROL_MASTER 0xA5
#define CONTROL_MASTER_WITH_DATA 0xA6
#define CONTROL_SLAVE 0x5A
#define CONTROL_SLAVE_WITH_DATA 0x5B
#define DATA_MASTER 0xAF
#define DATA_SLAVE 0xFA

#define MAX(a,b) (((a)>(b))?(a):(b))

struct header {
    union {
        uint16_t size;
        struct {
            uint8_t short_size;
            uint8_t long_size;
        } __attribute__((packed));
    };
    uint8_t magic;
    uint8_t checksum;
} __attribute__((packed));

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    static uint32_t s_last_time;
    uint32_t now = esp_timer_get_time();
    uint32_t diff = now - s_last_time;
    if (diff < 5) { // debounce
        return;
    }
    s_last_time = now;

    BaseType_t yield = false;
    struct eppp_handle *h = arg;
    xSemaphoreGiveFromISR(h->ready_semaphore, &yield);
    if (yield) {
        portYIELD_FROM_ISR();
    }
}

static esp_err_t deinit_master(esp_netif_t *netif)
{
    struct eppp_handle *h = esp_netif_get_io_driver(netif);
    ESP_RETURN_ON_ERROR(spi_bus_remove_device(h->spi_device), TAG, "Failed to remove SPI bus");
    ESP_RETURN_ON_ERROR(spi_bus_free(h->spi_host), TAG, "Failed to free SPI bus");
    return ESP_OK;
}

static esp_err_t init_master(struct eppp_config_spi_s *config, esp_netif_t *netif)
{
    struct eppp_handle *h = esp_netif_get_io_driver(netif);
    h->spi_host = config->host;
    h->gpio_intr = config->intr;
    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = config->mosi;
    bus_cfg.miso_io_num = config->miso;
    bus_cfg.sclk_io_num = config->sclk;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = 2000;
    bus_cfg.flags = 0;
    bus_cfg.intr_flags = 0;

    // TODO: Init and deinit SPI bus separately (per Kconfig?)
    if (spi_bus_initialize(config->host, &bus_cfg, SPI_DMA_CH_AUTO) != ESP_OK) {
        return ESP_FAIL;
    }

    spi_device_interface_config_t dev_cfg = {};
    dev_cfg.clock_speed_hz = config->freq;
    dev_cfg.mode = 0;
    dev_cfg.spics_io_num = config->cs;
    dev_cfg.cs_ena_pretrans = 0;
    dev_cfg.cs_ena_posttrans = 0;
    dev_cfg.duty_cycle_pos = 128;
    dev_cfg.input_delay_ns = 0;
    dev_cfg.pre_cb = NULL;
    dev_cfg.post_cb = NULL;
    dev_cfg.cs_ena_posttrans = 3;
    dev_cfg.queue_size = 3;

    if (spi_bus_add_device(config->host, &dev_cfg, &h->spi_device) != ESP_OK) {
        return ESP_FAIL;
    }

    //GPIO config for the handshake line.
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_POSEDGE,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
        .pin_bit_mask = BIT64(config->intr),
    };

    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_set_intr_type(config->intr, GPIO_INTR_POSEDGE);
    gpio_isr_handler_add(config->intr, gpio_isr_handler, esp_netif_get_io_driver(netif));
    return ESP_OK;
}

static void post_setup(spi_slave_transaction_t *trans)
{
    gpio_set_level((int)trans->user, 1);
}

static void post_trans(spi_slave_transaction_t *trans)
{
    gpio_set_level((int)trans->user, 0);
}

static esp_err_t deinit_slave(esp_netif_t *netif)
{
    struct eppp_handle *h = esp_netif_get_io_driver(netif);
    ESP_RETURN_ON_ERROR(spi_slave_free(h->spi_host), TAG, "Failed to free SPI slave host");
    ESP_RETURN_ON_ERROR(spi_bus_remove_device(h->spi_device), TAG, "Failed to remove SPI device");
    ESP_RETURN_ON_ERROR(spi_bus_free(h->spi_host), TAG, "Failed to free SPI bus");
    return ESP_OK;
}

static esp_err_t init_slave(struct eppp_config_spi_s *config, esp_netif_t *netif)
{
    struct eppp_handle *h = esp_netif_get_io_driver(netif);
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
        .post_trans_cb = post_trans,
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

union transaction {
    spi_transaction_t master;
    spi_slave_transaction_t slave;
};

typedef void (*set_transaction_t)(union transaction *t, size_t len, const void *tx_buffer, void *rx_buffer, int gpio_intr);
typedef esp_err_t (*perform_transaction_t)(union transaction *t, struct eppp_handle *h);

static void set_transaction_master(union transaction *t, size_t len, const void *tx_buffer, void *rx_buffer, int gpio_intr)
{
    t->master.length = len * 8;
    t->master.tx_buffer = tx_buffer;
    t->master.rx_buffer = rx_buffer;
}

static void set_transaction_slave(union transaction *t, size_t len, const void *tx_buffer, void *rx_buffer, int gpio_intr)
{
    t->slave.user = (void *)gpio_intr;
    t->slave.length = len * 8;
    t->slave.tx_buffer = tx_buffer;
    t->slave.rx_buffer = rx_buffer;
}

static esp_err_t perform_transaction_master(union transaction *t, struct eppp_handle *h)
{
    xSemaphoreTake(h->ready_semaphore, portMAX_DELAY); // Wait until slave is ready
    return spi_device_transmit(h->spi_device, &t->master);
}

static esp_err_t perform_transaction_slave(union transaction *t, struct eppp_handle *h)
{
    return spi_slave_transmit(h->spi_host, &t->slave, portMAX_DELAY);
}

esp_err_t eppp_perform(esp_netif_t *netif)
{
    static WORD_ALIGNED_ATTR uint8_t out_buf[TRANSFER_SIZE] = {};
    static WORD_ALIGNED_ATTR uint8_t in_buf[TRANSFER_SIZE] = {};

    struct eppp_handle *h = esp_netif_get_io_driver(netif);
    union transaction t;

    // Three types of frames (control, control+data, data), two roles (master, slave)
    const uint8_t FRAME_OUT_CTRL = h->role == EPPP_CLIENT ? CONTROL_MASTER : CONTROL_SLAVE;
    const uint8_t FRAME_OUT_CTRL_EX = h->role == EPPP_CLIENT ? CONTROL_MASTER_WITH_DATA : CONTROL_SLAVE_WITH_DATA;
    const uint8_t FRAME_OUT_DATA = h->role == EPPP_CLIENT ? DATA_MASTER : DATA_SLAVE;
    const uint8_t FRAME_IN_CTRL = h->role == EPPP_SERVER ? CONTROL_MASTER : CONTROL_SLAVE;
    const uint8_t FRAME_IN_CTRL_EX = h->role == EPPP_SERVER ? CONTROL_MASTER_WITH_DATA : CONTROL_SLAVE_WITH_DATA;
    const uint8_t FRAME_IN_DATA = h->role == EPPP_SERVER ? DATA_MASTER : DATA_SLAVE;
    // Two actions (prepare and perform transaction) for these two roles (master, slave)
    const set_transaction_t set_transaction = h->role == EPPP_CLIENT ? set_transaction_master : set_transaction_slave;
    const perform_transaction_t perform_transaction = h->role == EPPP_CLIENT ? perform_transaction_master : perform_transaction_slave;

    if (h->stop) {
        return ESP_ERR_TIMEOUT;
    }

    struct packet buf = { .len = 0 };
    struct header *head = (void *)out_buf;
    bool need_data_frame = false;
    size_t out_long_payload = 0;
    head->magic = FRAME_OUT_CTRL_EX;
    head->size = 0;
    head->checksum = 0;
    BaseType_t tx_queue_stat = xQueueReceive(h->out_queue, &buf, 0);
    if (tx_queue_stat == pdTRUE && buf.data) {
        if (buf.len > SHORT_PAYLOAD) {
            head->magic = FRAME_OUT_CTRL;
            head->size = buf.len;
            out_long_payload = buf.len;
            need_data_frame = true;
        } else {
            head->magic = FRAME_OUT_CTRL_EX;
            head->long_size = 0;
            head->short_size = buf.len;
            memcpy(out_buf + sizeof(struct header), buf.data, buf.len);
            free(buf.data);
        }
    }
    memset(&t, 0, sizeof(t));
    set_transaction(&t, CONTROL_SIZE, out_buf, in_buf, h->gpio_intr);
    for (int i = 0; i < sizeof(struct header) - 1; ++i) {
        head->checksum += out_buf[i];
    }
    esp_err_t ret = perform_transaction(&t, h);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_device_transmit failed");
        return ESP_FAIL;
    }
    head = (void *)in_buf;
    uint8_t checksum = 0;
    for (int i = 0; i < sizeof(struct header) - 1; ++i) {
        checksum += in_buf[i];
    }
    if (checksum != head->checksum) {
        ESP_LOGE(TAG, "Wrong checksum");
        return ESP_FAIL;
    }
    if (head->magic != FRAME_IN_CTRL && head->magic != FRAME_IN_CTRL_EX) {
        ESP_LOGE(TAG, "Wrong magic");
        return ESP_FAIL;
    }
    if (head->magic == FRAME_IN_CTRL_EX && head->short_size > 0) {
        esp_netif_receive(netif, in_buf + sizeof(struct header), head->short_size, NULL);
    }
    size_t in_long_payload = 0;
    if (head->magic == FRAME_IN_CTRL) {
        need_data_frame = true;
        in_long_payload = head->size;
    }
    if (!need_data_frame) {
        return ESP_OK;
    }

    // now, we need data frame
    head = (void *)out_buf;
    head->magic = FRAME_OUT_DATA;
    head->size = out_long_payload;
    head->checksum = 0;
    for (int i = 0; i < sizeof(struct header) - 1; ++i) {
        head->checksum += out_buf[i];
    }
    if (head->size > 0) {
        memcpy(out_buf + sizeof(struct header), buf.data, buf.len);
//            ESP_LOG_BUFFER_HEXDUMP(TAG, out_buf + sizeof(struct header), head->size, ESP_LOG_INFO);
        free(buf.data);
    }

    memset(&t, 0, sizeof(t));
    set_transaction(&t, MAX(in_long_payload, out_long_payload) + sizeof(struct header), out_buf, in_buf, h->gpio_intr);

    ret = perform_transaction(&t, h);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_device_transmit failed");
        return ESP_FAIL;
    }
    head = (void *)in_buf;
    checksum = 0;
    for (int i = 0; i < sizeof(struct header) - 1; ++i) {
        checksum += in_buf[i];
    }
    if (checksum != head->checksum) {
        ESP_LOGE(TAG, "Wrong checksum");
        return ESP_FAIL;
    }
    if (head->magic != FRAME_IN_DATA) {
        ESP_LOGE(TAG, "Wrong magic");
        return ESP_FAIL;
    }

    if (head->size > 0) {
        ESP_LOG_BUFFER_HEXDUMP(TAG, in_buf + sizeof(struct header), head->size, ESP_LOG_VERBOSE);
        esp_netif_receive(netif, in_buf + sizeof(struct header), head->size, NULL);
    }
    return ESP_OK;
}

static void ppp_task(void *args)
{
    esp_netif_t *netif = args;
    while (eppp_perform(netif) != ESP_ERR_TIMEOUT) {}
    struct eppp_handle *h = esp_netif_get_io_driver(netif);
    h->exited = true;
    vTaskDelete(NULL);
}

#elif CONFIG_EPPP_LINK_DEVICE_UART
#define BUF_SIZE (1024)

static esp_err_t init_uart(struct eppp_handle *h, eppp_config_t *config)
{
    h->uart_port = config->uart.port;
    uart_config_t uart_config = {};
    uart_config.baud_rate = config->uart.baud;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity    = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_DEFAULT;

    ESP_RETURN_ON_ERROR(uart_driver_install(h->uart_port, config->uart.rx_buffer_size, 0, config->uart.queue_size, &h->uart_event_queue, 0), TAG, "Failed to install UART");
    ESP_RETURN_ON_ERROR(uart_param_config(h->uart_port, &uart_config), TAG, "Failed to set params");
    ESP_RETURN_ON_ERROR(uart_set_pin(h->uart_port, config->uart.tx_io, config->uart.rx_io, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE), TAG, "Failed to set UART pins");
    ESP_RETURN_ON_ERROR(uart_set_rx_timeout(h->uart_port, 1), TAG, "Failed to set UART Rx timeout");
    return ESP_OK;
}

static void deinit_uart(struct eppp_handle *h)
{
    uart_driver_delete(h->uart_port);
}

esp_err_t eppp_perform(esp_netif_t *netif)
{
    static uint8_t buffer[BUF_SIZE] = {};
    struct eppp_handle *h = esp_netif_get_io_driver(netif);
    uart_event_t event = {};
    if (h->stop) {
        return ESP_ERR_TIMEOUT;
    }

    if (xQueueReceive(h->uart_event_queue, &event, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_OK;
    }
    if (event.type == UART_DATA) {
        size_t len;
        uart_get_buffered_data_len(h->uart_port, &len);
        if (len) {
            len = uart_read_bytes(h->uart_port, buffer, BUF_SIZE, 0);
            ESP_LOG_BUFFER_HEXDUMP("ppp_uart_recv", buffer, len, ESP_LOG_VERBOSE);
            esp_netif_receive(netif, buffer, len, NULL);
        }
    } else {
        ESP_LOGW(TAG, "Received UART event: %d", event.type);
    }
    return ESP_OK;
}

static void ppp_task(void *args)
{
    esp_netif_t *netif = args;
    while (eppp_perform(netif) == ESP_OK) {}
    struct eppp_handle *h = esp_netif_get_io_driver(netif);
    h->exited = true;
    vTaskDelete(NULL);
}

#endif // CONFIG_EPPP_LINK_DEVICE_SPI / UART

static bool have_some_eppp_netif(esp_netif_t *netif, void *ctx)
{
    return get_netif_num(netif) > 0;
}

static void remove_handlers(void)
{
    esp_netif_t *netif = esp_netif_find_if(have_some_eppp_netif, NULL);
    if (netif == NULL) {
        // if EPPP netif in the system, we cleanup the statics
        vEventGroupDelete(s_event_group);
        s_event_group = NULL;
        esp_event_handler_unregister(IP_EVENT, ESP_EVENT_ANY_ID, on_ip_event);
        esp_event_handler_unregister(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, on_ppp_event);
    }
}

void eppp_deinit(esp_netif_t *netif)
{
#if CONFIG_EPPP_LINK_DEVICE_SPI
    struct eppp_handle *h = esp_netif_get_io_driver(netif);
    if (h->role == EPPP_CLIENT) {
        deinit_master(netif);
    } else {
        deinit_slave(netif);
    }
#elif CONFIG_EPPP_LINK_DEVICE_UART
    deinit_uart(esp_netif_get_io_driver(netif));
#endif
    netif_deinit(netif);
}

esp_netif_t *eppp_init(enum eppp_type role, eppp_config_t *config)
{
    esp_netif_t *netif = netif_init(role);
    if (!netif) {
        ESP_LOGE(TAG, "Failed to initialize PPP netif");
        remove_handlers();
        return NULL;
    }
    esp_netif_ppp_config_t netif_params;
    ESP_ERROR_CHECK(esp_netif_ppp_get_params(netif, &netif_params));
    netif_params.ppp_our_ip4_addr = config->ppp.our_ip4_addr;
    netif_params.ppp_their_ip4_addr = config->ppp.their_ip4_addr;
    netif_params.ppp_error_event_enabled = true;
    ESP_ERROR_CHECK(esp_netif_ppp_set_params(netif, &netif_params));
#if CONFIG_EPPP_LINK_DEVICE_SPI
    if (role == EPPP_CLIENT) {
        init_master(&config->spi, netif);

        // as a client, try to actively connect (not waiting for server's interrupt)
        struct eppp_handle *h = esp_netif_get_io_driver(netif);
        xSemaphoreGive(h->ready_semaphore);
    } else {
        init_slave(&config->spi, netif);

    }
#elif CONFIG_EPPP_LINK_DEVICE_UART
    init_uart(esp_netif_get_io_driver(netif), config);
#endif
    return netif;
}

esp_netif_t *eppp_open(enum eppp_type role, eppp_config_t *config, TickType_t connect_timeout)
{
#if CONFIG_EPPP_LINK_DEVICE_UART
    if (config->transport != EPPP_TRANSPORT_UART) {
        ESP_LOGE(TAG, "Invalid transport: UART device must be enabled in Kconfig");
        return NULL;
    }
#endif
#if CONFIG_EPPP_LINK_DEVICE_SPI
    if (config->transport != EPPP_TRANSPORT_SPI) {
        ESP_LOGE(TAG, "Invalid transport: SPI device must be enabled in Kconfig");
        return NULL;
    }
#endif

    if (config->task.run_task == false) {
        ESP_LOGE(TAG, "task.run_task == false is invalid in this API. Please use eppp_init()");
        return NULL;
    }
    if (s_event_group == NULL) {
        s_event_group = xEventGroupCreate();
        if (esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, on_ip_event, NULL) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register IP event handler");
            remove_handlers();
            return NULL;
        }
        if (esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, on_ppp_event, NULL) !=  ESP_OK) {
            ESP_LOGE(TAG, "Failed to register PPP status handler");
            remove_handlers();
            return NULL;
        }
    }
    esp_netif_t *netif = eppp_init(role, config);
    if (!netif) {
        remove_handlers();
        return NULL;
    }

    eppp_netif_start(netif);

    if (xTaskCreate(ppp_task, "ppp connect", config->task.stack_size, netif, config->task.priority, NULL) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to create a ppp connection task");
        eppp_deinit(netif);
        return NULL;
    }
    int netif_cnt = get_netif_num(netif);
    if (netif_cnt < 0) {
        eppp_close(netif);
        return NULL;
    }
    ESP_LOGI(TAG, "Waiting for IP address %d", netif_cnt);
    EventBits_t bits = xEventGroupWaitBits(s_event_group, CONNECT_BITS << (netif_cnt * 2), pdFALSE, pdFALSE, connect_timeout);
    if (bits & (CONNECTION_FAILED << (netif_cnt * 2))) {
        ESP_LOGE(TAG, "Connection failed!");
        eppp_close(netif);
        return NULL;
    }
    ESP_LOGI(TAG, "Connected! %d", netif_cnt);
    return netif;
}

esp_netif_t *eppp_connect(eppp_config_t *config)
{
    return eppp_open(EPPP_CLIENT, config, portMAX_DELAY);
}

esp_netif_t *eppp_listen(eppp_config_t *config)
{
    return eppp_open(EPPP_SERVER, config, portMAX_DELAY);
}

void eppp_close(esp_netif_t *netif)
{
    struct eppp_handle *h = esp_netif_get_io_driver(netif);
    if (eppp_netif_stop(netif, pdMS_TO_TICKS(60000)) != ESP_OK) {
        ESP_LOGE(TAG, "Network didn't exit cleanly");
    }
    h->stop = true;
    for (int wait = 0; wait < 100; wait++) {
        vTaskDelay(pdMS_TO_TICKS(10));
        if (h->exited) {
            break;
        }
    }
    if (!h->exited) {
        ESP_LOGE(TAG, "Cannot stop ppp_task");
    }
    eppp_deinit(netif);
    remove_handlers();
}
