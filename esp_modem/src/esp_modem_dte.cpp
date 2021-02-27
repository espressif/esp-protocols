//
// Created by david on 2/24/21.
//

#include "cxx_include/esp_modem_dte.hpp"
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_modem.h"
#include "esp_modem_dce.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "esp_modem_internal.h"
#include "esp_modem_dte_internal.h"
#include "esp_modem_dte_config.h"
#include <memory>
#include <utility>

#define ESP_MODEM_EVENT_QUEUE_SIZE (16)
#define MIN_PATTERN_INTERVAL (9)
#define MIN_POST_IDLE (0)
#define MIN_PRE_IDLE (0)


static const char *TAG = "dte_uart";

//class uart_terminal;

//class dte {
//public:
//    esp_err_t init(std::unique_ptr<terminal> t);
//
//private:
//    std::unique_ptr<terminal> m_terminal;
//
//};

struct uart_task {
    explicit uart_task(size_t stack_size, size_t priority, void* task_param, TaskFunction_t task_function):
            task_handle(nullptr)
    {
        BaseType_t ret = xTaskCreate(task_function, "uart_task", stack_size, task_param, priority, &task_handle);
        throw_if_false(ret == pdTRUE, "create uart event task failed");
    }
    ~uart_task()
    {
        if (task_handle) vTaskDelete(task_handle);
    }

    TaskHandle_t task_handle;       /*!< UART event task handle */
};



struct uart_event_loop {
    explicit uart_event_loop(): event_loop_hdl(nullptr)
    {
        esp_event_loop_args_t loop_args = {};
        loop_args.queue_size = ESP_MODEM_EVENT_QUEUE_SIZE;
        loop_args.task_name = nullptr;
        throw_if_esp_fail(esp_event_loop_create(&loop_args, &event_loop_hdl), "create event loop failed");
    }
    void run() { esp_event_loop_run(event_loop_hdl, pdMS_TO_TICKS(0)); }

    ~uart_event_loop() { if (event_loop_hdl) esp_event_loop_delete(event_loop_hdl); }

    esp_event_loop_handle_t event_loop_hdl;
};

struct uart_resource {
    explicit uart_resource(const struct dte_config *config);

    ~uart_resource();

    bool get_event(uart_event_t& event, uint32_t time_ms)
    {
        return xQueueReceive(event_queue, &event, pdMS_TO_TICKS(time_ms));
    }
    void reset_events()
    {
        uart_flush_input(port);
        xQueueReset(event_queue);
    }

    uart_port_t port;                  /*!< UART port */
    QueueHandle_t event_queue;              /*!< UART event queue handle */
//    esp_modem_on_receive receive_cb;        /*!< ptr to data reception */
//    void *receive_cb_ctx;                   /*!< ptr to rx fn context data */
    int line_buffer_size;                   /*!< line buffer size in command mode */
    int pattern_queue_size;                 /*!< UART pattern queue size */
};

class uart_terminal: public terminal {
public:
    explicit  uart_terminal(const struct dte_config *config):
            uart(config), event_loop(), signal(),
            task_handle(config->event_task_stack_size, config->event_task_priority, this, s_task) {}

    ~uart_terminal() override = default;
    void start() override
    {
        signal.set(TASK_START);
    }
    void stop() override
    {
        signal.set(TASK_STOP);
    }
//    { ESP_LOGE(TAG, "uart_terminal destruct"); }

    int write(uint8_t *data, size_t len) override;
    int read(uint8_t *data, size_t len) override;

private:
    static void s_task(void * task_param)
    {
        auto t = static_cast<uart_terminal*>(task_param);
        t->task();
        vTaskDelete(NULL);
    }
    void task();

    const size_t TASK_INIT = BIT0;
    const size_t TASK_START = BIT1;
    const size_t TASK_STOP = BIT2;


    uart_resource uart;
    uart_event_loop event_loop;
    signal_group signal;
    uart_task task_handle;

};


uart_resource::~uart_resource()
{
    if (port >= UART_NUM_0 && port < UART_NUM_MAX) {
        uart_driver_delete(port);
    }
}




uart_resource::uart_resource(const struct dte_config *config):
        port(-1)
//        buffer(std::make_unique<uint8_t[]>(config->line_buffer_size))
{
    esp_err_t res;

    line_buffer_size = config->line_buffer_size;

    /* TODO: Bind methods */

    /* Config UART */
    uart_config_t uart_config = {};
    uart_config.baud_rate = config->baud_rate;
    uart_config.data_bits = config->data_bits;
    uart_config.parity = config->parity;
    uart_config.stop_bits = config->stop_bits;
    uart_config.flow_ctrl = (config->flow_control == ESP_MODEM_FLOW_CONTROL_HW) ? UART_HW_FLOWCTRL_CTS_RTS
                                                                     : UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_REF_TICK;

    throw_if_esp_fail(uart_param_config(config->port_num, &uart_config), "config uart parameter failed");

    if (config->flow_control == ESP_MODEM_FLOW_CONTROL_HW) {
        res = uart_set_pin(config->port_num, config->tx_io_num, config->rx_io_num,
                           config->rts_io_num, config->cts_io_num);
    } else {
        res = uart_set_pin(config->port_num, config->tx_io_num, config->rx_io_num,
                           UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    }
    throw_if_esp_fail(res, "config uart gpio failed");
    /* Set flow control threshold */
    if (config->flow_control == ESP_MODEM_FLOW_CONTROL_HW) {
        res = uart_set_hw_flow_ctrl(config->port_num, UART_HW_FLOWCTRL_CTS_RTS, UART_FIFO_LEN - 8);
    } else if (config->flow_control == ESP_MODEM_FLOW_CONTROL_SW) {
        res = uart_set_sw_flow_ctrl(config->port_num, true, 8, UART_FIFO_LEN - 8);
    }
    throw_if_esp_fail(res, "config uart flow control failed");
    /* Install UART driver and get event queue used inside driver */
    res = uart_driver_install(config->port_num, config->rx_buffer_size, config->tx_buffer_size,
                              config->event_queue_size, &(event_queue), 0);
    throw_if_esp_fail(res, "install uart driver failed");
    throw_if_esp_fail(uart_set_rx_timeout(config->port_num, 1), "set rx timeout failed");

    /* Set pattern interrupt, used to detect the end of a line. */
//    res = uart_enable_pattern_det_baud_intr(config->port_num, '\n', 1, MIN_PATTERN_INTERVAL, MIN_POST_IDLE, MIN_PRE_IDLE);
    /* Set pattern queue size */
//    pattern_queue_size = config->pattern_queue_size;
//    res |= uart_pattern_queue_reset(config->port_num, config->pattern_queue_size);
    /* Starting in command mode -> explicitly disable RX interrupt */
//    uart_disable_rx_intr(config->port_num);
    uart_set_rx_full_threshold(config->port_num, 64);
    throw_if_esp_fail(res, "config uart pattern failed");
    /* mark UART as initialized */
    port = config->port_num;
}

std::unique_ptr<terminal> create_uart_terminal(const struct dte_config *config)
{
    try {
        auto term = std::make_unique<uart_terminal>(config);
        term->start();
        return term;
    } catch (std::bad_alloc& e) {
        ESP_LOGE(TAG, "Out of memory");
        return nullptr;
    } catch (esp_err_exception& e) {
        esp_err_t err = e.get_err_t();
        ESP_LOGE(TAG, "Error occurred during UART term init: %d", err);
        ESP_LOGE(TAG, "%s", e.what());
        return nullptr;
    }

}

std::shared_ptr<dte> create_dte(const struct dte_config *config)
{
    try {
//        auto term = std::make_unique<dte>(std::make_unique<uart_terminal>(config));
        auto term = std::make_shared<dte>(std::make_unique<uart_terminal>(config));
        return term;
    } catch (std::bad_alloc& e) {
        ESP_LOGE(TAG, "Out of memory");
        return nullptr;
    } catch (esp_err_exception& e) {
        esp_err_t err = e.get_err_t();
        ESP_LOGE(TAG, "Error occurred during UART term init: %d", err);
        ESP_LOGE(TAG, "%s", e.what());
        return nullptr;
    }

}

std::unique_ptr<dce> create_dce(const std::shared_ptr<dte>& e, esp_netif_t *netif)
{
    try {
        return std::make_unique<dce>(e, netif);
    } catch (std::bad_alloc& e) {
        ESP_LOGE(TAG, "Out of memory");
        return nullptr;
    } catch (esp_err_exception& e) {
        esp_err_t err = e.get_err_t();
        ESP_LOGE(TAG, "Error occurred during UART term init: %d", err);
        ESP_LOGE(TAG, "%s", e.what());
        return nullptr;
    }
}


void uart_terminal::task()
{
    uart_event_t event;
    size_t len;
    signal.set(TASK_INIT);
    signal.wait_any(TASK_START | TASK_STOP, portMAX_DELAY);
    if (signal.is_any(TASK_STOP)) {
        return; // deletes the static task
    }
    while(signal.is_any(TASK_START)) {
        event_loop.run();
        if (uart.get_event(event, 100)) {
            switch (event.type) {
                case UART_DATA:
                    ESP_LOGI(TAG, "UART_DATA");
                    uart_get_buffered_data_len(uart.port, &len);
                    ESP_LOGI(TAG, "UART_DATA len=%d, on_data=%d", len, (bool)on_data);
                    if (len && on_data) {
                        on_data(len);
                    }
                    break;
                case UART_FIFO_OVF:
                    ESP_LOGW(TAG, "HW FIFO Overflow");
                    uart.reset_events();
                    break;
                case UART_BUFFER_FULL:
                    ESP_LOGW(TAG, "Ring Buffer Full");
                    uart.reset_events();
                    break;
                case UART_BREAK:
                    ESP_LOGW(TAG, "Rx Break");
                    break;
                case UART_PARITY_ERR:
                    ESP_LOGE(TAG, "Parity Error");
                    break;
                case UART_FRAME_ERR:
                    ESP_LOGE(TAG, "Frame Error");
                    break;
                case UART_PATTERN_DET:
                    ESP_LOGI(TAG, "UART_PATTERN_DET");
                    break;
                default:
                    ESP_LOGW(TAG, "unknown uart event type: %d", event.type);
                    break;
            }
        }

//        ESP_LOGI(TAG, "uart_event_task_entry");
//        vTaskDelay(pdMS_TO_TICKS(200));

    }
}

int uart_terminal::read(uint8_t *data, size_t len)
{
    size_t length = 0;
    uart_get_buffered_data_len(uart.port, &length);
    if (length > 0) {
        return uart_read_bytes(uart.port, data, length, portMAX_DELAY);
    }
    return 0;
}

int uart_terminal::write(uint8_t *data, size_t len)
{
    return uart_write_bytes(uart.port, data, len);
}

dte::dte(std::unique_ptr<terminal> terminal):
    buffer_size(DTE_BUFFER_SIZE), consumed(0),
    buffer(std::make_unique<uint8_t[]>(buffer_size)),
    term(std::move(terminal)), mode(dte_mode::UNDEF) {}


bool dte::send_command(const std::string& command, got_line_cb got_line, uint32_t time_ms)
{
    term->write((uint8_t *)command.c_str(), command.length());
    term->set_data_cb([&](size_t len){
        auto data_to_read = std::min(len, buffer_size - consumed);
        auto data = buffer.get() + consumed;
        auto actual_len = term->read(data, data_to_read);
        consumed += actual_len;
        if (memchr(data, '\n', actual_len)) {
            ESP_LOGD("in the lambda", "FOUND");
            if (got_line(buffer.get(), consumed)) {
                signal.set(GOT_LINE);
            }
        }
    });
    auto res = signal.wait(GOT_LINE, time_ms);
    consumed = 0;
    return res;
}

dce::dce(std::shared_ptr<dte> e, esp_netif_t * netif):
dce_dte(e), ppp_netif(e, netif)
{ }

