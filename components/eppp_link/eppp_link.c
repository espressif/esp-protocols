/*
 * SPDX-FileCopyrightText: 2019-2023 Espressif Systems (Shanghai) CO LTD
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
#include "eppp_link_types.h"

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

#if CONFIG_EPPP_LINK_DEVICE_SPI
static spi_device_handle_t s_spi_device;
#define SPI_HOST     SPI2_HOST
#define GPIO_MOSI    11
#define GPIO_MISO    13
#define GPIO_SCLK    12
#define GPIO_CS      10
#define GPIO_INTR    2
#endif // CONFIG_EPPP_LINK_DEVICE_SPI

enum eppp_type {
    EPPP_SERVER,
    EPPP_CLIENT,
};

struct eppp_handle {
    QueueHandle_t out_queue;
#if CONFIG_EPPP_LINK_DEVICE_SPI
    QueueHandle_t ready_semaphore;
#elif CONFIG_EPPP_LINK_DEVICE_UART
    QueueHandle_t uart_event_queue;
#endif
    esp_netif_t *netif;
    enum eppp_type role;
};

struct packet {
    size_t len;
    uint8_t *data;
};


static esp_err_t transmit(void *h, void *buffer, size_t len)
{
#if CONFIG_EPPP_LINK_DEVICE_SPI
#define MAX_PAYLOAD 1600
    struct eppp_handle *handle = h;
    struct packet buf = { };

    uint8_t *current_buffer = buffer;
    size_t remaining = len;
    do {
        size_t batch = remaining > MAX_PAYLOAD ? MAX_PAYLOAD : remaining;
        buf.data = malloc(batch);
        buf.len = batch;
        remaining -= batch;
        memcpy(buf.data, current_buffer, batch);
        current_buffer += batch;
        BaseType_t ret = xQueueSend(handle->out_queue, &buf, pdMS_TO_TICKS(10));
        if (ret != pdTRUE) {
            ESP_LOGE(TAG, "Failed to queue packet to slave!");
        }
    } while (remaining > 0);
#elif CONFIG_EPPP_LINK_DEVICE_UART
    uart_write_bytes(UART_NUM_1, buffer, len);
#endif
    return ESP_OK;
}

static esp_netif_t *netif_init(enum eppp_type role)
{
    static int s_eppp_netif_count = 0; // used as a suffix for the netif key
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
    h->out_queue = xQueueCreate(CONFIG_EPPP_LINK_PACKET_QUEUE_SIZE, sizeof(struct packet));
    if (!h->out_queue) {
        ESP_LOGE(TAG, "Failed to create the packet queue");
        free(h);
        return NULL;
    }
    h->role = role;
#if CONFIG_EPPP_LINK_DEVICE_SPI
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
        vQueueDelete(h->out_queue);
        free(h);
        return NULL;
    }
    return netif;

}

static esp_err_t netif_start(esp_netif_t *netif)
{
    esp_netif_action_start(netif, 0, 0, 0);
    esp_netif_action_connected(netif, 0, 0, 0);
    return ESP_OK;
}

static void on_ip_event(void *arg, esp_event_base_t base, int32_t event_id, void *data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)data;
    esp_netif_t *netif = event->esp_netif;
    if (event_id == IP_EVENT_PPP_GOT_IP) {
        ESP_LOGI(TAG, "Got IPv4 event: Interface \"%s\" address: " IPSTR, esp_netif_get_desc(event->esp_netif), IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_event_group, GOT_IPV4);
    } else if (event_id == IP_EVENT_PPP_LOST_IP) {
        ESP_LOGI(TAG, "Disconnect from PPP Server");
        s_retry_num++;
        if (s_retry_num > CONFIG_EPPP_LINK_CONN_MAX_RETRY) {
            ESP_LOGE(TAG, "PPP Connection failed %d times, stop reconnecting.", s_retry_num);
            xEventGroupSetBits(s_event_group, CONNECTION_FAILED);
        } else {
            ESP_LOGI(TAG, "PPP Connection failed %d times, try to reconnect.", s_retry_num);
            netif_start(netif);
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

static esp_err_t init_master(spi_device_handle_t *spi, esp_netif_t *netif)
{
    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = GPIO_MOSI;
    bus_cfg.miso_io_num = GPIO_MISO;
    bus_cfg.sclk_io_num = GPIO_SCLK;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = 14000;
    bus_cfg.flags = 0;
    bus_cfg.intr_flags = 0;

    if (spi_bus_initialize(SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO) != ESP_OK) {
        return ESP_FAIL;
    }

    spi_device_interface_config_t dev_cfg = {};
    dev_cfg.clock_speed_hz = 40 * 1000 * 1000;
    dev_cfg.mode = 0;
    dev_cfg.spics_io_num = GPIO_CS;
    dev_cfg.cs_ena_pretrans = 0;
    dev_cfg.cs_ena_posttrans = 0;
    dev_cfg.duty_cycle_pos = 128;
    dev_cfg.input_delay_ns = 0;
    dev_cfg.pre_cb = NULL;
    dev_cfg.post_cb = NULL;
    dev_cfg.cs_ena_posttrans = 3;
    dev_cfg.queue_size = 3;

    if (spi_bus_add_device(SPI_HOST, &dev_cfg, spi) != ESP_OK) {
        return ESP_FAIL;
    }

    //GPIO config for the handshake line.
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_POSEDGE,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
        .pin_bit_mask = BIT64(GPIO_INTR),
    };

    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_set_intr_type(GPIO_INTR, GPIO_INTR_POSEDGE);
    gpio_isr_handler_add(GPIO_INTR, gpio_isr_handler, esp_netif_get_io_driver(netif));
    return ESP_OK;
}

static void post_setup(spi_slave_transaction_t *trans)
{
    gpio_set_level(GPIO_INTR, 1);
}

static void post_trans(spi_slave_transaction_t *trans)
{
    gpio_set_level(GPIO_INTR, 0);
}

static esp_err_t init_slave(spi_device_handle_t *spi, esp_netif_t *netif)
{
    spi_bus_config_t bus_cfg = {};
    bus_cfg.mosi_io_num = GPIO_MOSI;
    bus_cfg.miso_io_num = GPIO_MISO;
    bus_cfg.sclk_io_num = GPIO_SCLK;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.flags = 0;
    bus_cfg.intr_flags = 0;

    //Configuration for the SPI slave interface
    spi_slave_interface_config_t slvcfg = {
        .mode = 0,
        .spics_io_num = GPIO_CS,
        .queue_size = 3,
        .flags = 0,
        .post_setup_cb = post_setup,
        .post_trans_cb = post_trans
    };

    //Configuration for the handshake line
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = BIT64(GPIO_INTR),
    };

    gpio_config(&io_conf);
    gpio_set_pull_mode(GPIO_MOSI, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(GPIO_SCLK, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(GPIO_CS, GPIO_PULLUP_ONLY);

    //Initialize SPI slave interface
    if (spi_slave_initialize(SPI_HOST, &bus_cfg, &slvcfg, SPI_DMA_CH_AUTO) != ESP_OK) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

union transaction {
    spi_transaction_t master;
    spi_slave_transaction_t slave;
};

typedef void (*set_transaction_t)(union transaction *t, size_t len, const void *tx_buffer, void *rx_buffer);
typedef esp_err_t (*perform_transaction_t)(union transaction *t, struct eppp_handle *h);

static void set_transaction_master(union transaction *t, size_t len, const void *tx_buffer, void *rx_buffer)
{
    t->master.length = len * 8;
    t->master.tx_buffer = tx_buffer;
    t->master.rx_buffer = rx_buffer;
}

static void set_transaction_slave(union transaction *t, size_t len, const void *tx_buffer, void *rx_buffer)
{
    t->slave.length = len * 8;
    t->slave.tx_buffer = tx_buffer;
    t->slave.rx_buffer = rx_buffer;
}

static esp_err_t perform_transaction_master(union transaction *t, struct eppp_handle *h)
{
    xSemaphoreTake(h->ready_semaphore, portMAX_DELAY); // Wait until slave is ready
    return spi_device_transmit(s_spi_device, &t->master);
}

static esp_err_t perform_transaction_slave(union transaction *t, struct eppp_handle *h)
{
    return spi_slave_transmit(SPI_HOST, &t->slave, portMAX_DELAY);
}

_Noreturn static void ppp_task(void *args)
{
    static WORD_ALIGNED_ATTR uint8_t out_buf[TRANSFER_SIZE] = {};
    static WORD_ALIGNED_ATTR uint8_t in_buf[TRANSFER_SIZE] = {};

    esp_netif_t *netif = args;
    struct eppp_handle *h = esp_netif_get_io_driver(netif);
    union transaction t;

    const uint8_t FRAME_OUT_CTRL = h->role == EPPP_CLIENT ? CONTROL_MASTER : CONTROL_SLAVE;
    const uint8_t FRAME_OUT_CTRL_EX = h->role == EPPP_CLIENT ? CONTROL_MASTER_WITH_DATA : CONTROL_SLAVE_WITH_DATA;
    const uint8_t FRAME_OUT_DATA = h->role == EPPP_CLIENT ? DATA_MASTER : DATA_SLAVE;
    const uint8_t FRAME_IN_CTRL = h->role == EPPP_SERVER ? CONTROL_MASTER : CONTROL_SLAVE;
    const uint8_t FRAME_IN_CTRL_EX = h->role == EPPP_SERVER ? CONTROL_MASTER_WITH_DATA : CONTROL_SLAVE_WITH_DATA;
    const uint8_t FRAME_IN_DATA = h->role == EPPP_SERVER ? DATA_MASTER : DATA_SLAVE;
    const set_transaction_t set_transaction = h->role == EPPP_CLIENT ? set_transaction_master : set_transaction_slave;
    const perform_transaction_t perform_transaction = h->role == EPPP_CLIENT ? perform_transaction_master : perform_transaction_slave;

    if (h->role == EPPP_CLIENT) {
        // as a client, try to actively connect (not waiting for server's interrupt)
        xSemaphoreGive(h->ready_semaphore);
    }
    while (1) {
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
//                printf("need_data_frame %d\n", buf.len);
            } else {
                head->magic = FRAME_OUT_CTRL_EX;
                head->long_size = 0;
                head->short_size = buf.len;
                memcpy(out_buf + sizeof(struct header), buf.data, buf.len);
                free(buf.data);
            }
        }
        memset(&t, 0, sizeof(t));
        set_transaction(&t, CONTROL_SIZE, out_buf, in_buf);
        for (int i = 0; i < sizeof(struct header) - 1; ++i) {
            head->checksum += out_buf[i];
        }
        esp_err_t ret = perform_transaction(&t, h);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "spi_device_transmit failed");
            continue;
        }
        head = (void *)in_buf;
        uint8_t checksum = 0;
        for (int i = 0; i < sizeof(struct header) - 1; ++i) {
            checksum += in_buf[i];
        }
        if (checksum != head->checksum) {
            ESP_LOGE(TAG, "Wrong checksum");
            continue;
        }
//        printf("MAGIC: %x\n", head->magic);
        if (head->magic != FRAME_IN_CTRL && head->magic != FRAME_IN_CTRL_EX) {
            ESP_LOGE(TAG, "Wrong magic");
            continue;
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
            continue;
        }
        // now, we need data frame
//        printf("performing data frame %d %d\n", out_long_payload, buf.len);
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
        set_transaction(&t, MAX(in_long_payload, out_long_payload) + sizeof(struct header), out_buf, in_buf);

        ret = perform_transaction(&t, h);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "spi_device_transmit failed");
            continue;
        }
        head = (void *)in_buf;
        checksum = 0;
        for (int i = 0; i < sizeof(struct header) - 1; ++i) {
            checksum += in_buf[i];
        }
        if (checksum != head->checksum) {
            ESP_LOGE(TAG, "Wrong checksum");
            continue;
        }
        if (head->magic != FRAME_IN_DATA) {
            ESP_LOGE(TAG, "Wrong magic");
            continue;
        }
//        printf("got size %d\n", head->size);

        if (head->size > 0) {
//            ESP_LOG_BUFFER_HEXDUMP(TAG, in_buf + sizeof(struct header), head->size, ESP_LOG_INFO);
            esp_netif_receive(netif, in_buf + sizeof(struct header), head->size, NULL);
        }
    }
}
#elif CONFIG_EPPP_LINK_DEVICE_UART
#define BUF_SIZE (1024)
#define UART_TX_CLIENT_TO_SERVER 10
#define UART_TX_SERVER_TO_CLIENT 11
#define UART_BAUDRATE 4000000
#define UART_QUEUE_SIZE 16

static esp_err_t init_uart(struct eppp_handle *h)
{
    uart_config_t uart_config = {};
    uart_config.baud_rate = UART_BAUDRATE;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity    = UART_PARITY_DISABLE;
    uart_config.stop_bits = UART_STOP_BITS_1;
    uart_config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_DEFAULT;

    ESP_RETURN_ON_ERROR(uart_driver_install(UART_NUM_1, BUF_SIZE, 0, UART_QUEUE_SIZE, &h->uart_event_queue, 0), TAG, "Failed to install UART");
    ESP_RETURN_ON_ERROR(uart_param_config(UART_NUM_1, &uart_config), TAG, "Failed to set params");
    int tx_io_num = h->role == EPPP_CLIENT ? UART_TX_CLIENT_TO_SERVER : UART_TX_SERVER_TO_CLIENT;
    int rx_io_num = h->role == EPPP_CLIENT ? UART_TX_SERVER_TO_CLIENT : UART_TX_CLIENT_TO_SERVER;
    ESP_RETURN_ON_ERROR(uart_set_pin(UART_NUM_1, tx_io_num, rx_io_num, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE), TAG, "Failed to set UART pins");
    ESP_RETURN_ON_ERROR(uart_set_rx_timeout(UART_NUM_1, 1), TAG, "Failed to set UART Rx timeout");
    return ESP_OK;
}

_Noreturn static void ppp_task(void *args)
{
    static uint8_t buffer[BUF_SIZE] = {};

    esp_netif_t *netif = args;
    struct eppp_handle *h = esp_netif_get_io_driver(netif);
    uart_event_t event;
    while (1) {
        xQueueReceive(h->uart_event_queue, &event, pdMS_TO_TICKS(pdMS_TO_TICKS(100)));
        if (event.type == UART_DATA) {
            size_t len;
            uart_get_buffered_data_len(UART_NUM_1, &len);
            if (len) {
                len = uart_read_bytes(UART_NUM_1, buffer, BUF_SIZE, 0);
                ESP_LOG_BUFFER_HEXDUMP("ppp_uart_recv", buffer, len, ESP_LOG_VERBOSE);
                esp_netif_receive(netif, buffer, len, NULL);
            }
        } else {
            ESP_LOGW(TAG, "Received UART event: %d", event.type);
        }
    }

}
#endif // CONFIG_EPPP_LINK_DEVICE_SPI / UART


static esp_netif_t *default_setup(enum eppp_type role)
{
    s_event_group = xEventGroupCreate();
    if (esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, on_ip_event, NULL) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register IP event handler");
        return NULL;
    }
    esp_netif_t *netif = netif_init(role);
    if (!netif) {
        ESP_LOGE(TAG, "Failed to initialize PPP netif");
        vEventGroupDelete(s_event_group);
        return NULL;
    }
    esp_event_handler_register(IP_EVENT, IP_EVENT_PPP_GOT_IP, esp_netif_action_connected, netif);
    esp_netif_ppp_config_t netif_params;
    ESP_ERROR_CHECK(esp_netif_ppp_get_params(netif, &netif_params));
    netif_params.ppp_our_ip4_addr = esp_netif_htonl(role == EPPP_SERVER ? CONFIG_EPPP_LINK_SERVER_IP : CONFIG_EPPP_LINK_CLIENT_IP);
    netif_params.ppp_their_ip4_addr = esp_netif_htonl(role == EPPP_SERVER ? CONFIG_EPPP_LINK_CLIENT_IP : CONFIG_EPPP_LINK_SERVER_IP);
    ESP_ERROR_CHECK(esp_netif_ppp_set_params(netif, &netif_params));
#if CONFIG_EPPP_LINK_DEVICE_SPI
    if (role == EPPP_CLIENT) {
        init_master(&s_spi_device, netif);
    } else {
        init_slave(&s_spi_device, netif);
    }
#elif CONFIG_EPPP_LINK_DEVICE_UART
    init_uart(esp_netif_get_io_driver(netif));
#endif

    netif_start(netif);

    if (xTaskCreate(ppp_task, "ppp connect", 4096, netif, 18, NULL) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to create a ppp connection task");
        return NULL;
    }
    ESP_LOGI(TAG, "Waiting for IP address");
    EventBits_t bits = xEventGroupWaitBits(s_event_group, CONNECT_BITS, pdFALSE, pdFALSE, portMAX_DELAY);
    if (bits & CONNECTION_FAILED) {
        ESP_LOGE(TAG, "Connection failed!");
        return NULL;
    }
    ESP_LOGI(TAG, "Connected!");
    return netif;
}

esp_netif_t *eppp_connect(void)
{
    return default_setup(EPPP_CLIENT);
}

esp_netif_t *eppp_listen(void)
{
    return default_setup(EPPP_SERVER);
}
