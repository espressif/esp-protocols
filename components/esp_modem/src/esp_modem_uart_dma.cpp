/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "driver/uhci.h"
#include "uart_compat.h"
#include "esp_modem_config.h"
#include "exception_stub.hpp"
#include "cxx_include/esp_modem_dte.hpp"
#include "uart_resource.hpp"
#include "esp_heap_caps.h"
#include <string.h>
#include <algorithm>

static const char *TAG = "uart_dma_terminal";

namespace esp_modem {

struct uart_dma_task {
    explicit uart_dma_task(size_t stack_size, size_t priority, void *task_param, TaskFunction_t task_function) :
        task_handle(nullptr)
    {
        BaseType_t ret = xTaskCreate(task_function, "uart_dma_task", stack_size, task_param, priority, &task_handle);
        ESP_MODEM_THROW_IF_FALSE(ret == pdTRUE, "create uart dma event task failed");
    }

    ~uart_dma_task()
    {
        if (task_handle) {
            vTaskDelete(task_handle);
        }
    }

    TaskHandle_t task_handle;       /*!< UART DMA event task handle */
};

class UartDmaTerminal : public Terminal {
public:
    explicit UartDmaTerminal(const esp_modem_dte_config *config) :
        event_queue(), uart(&config->uart_config, &event_queue, -1), signal(),
        task_handle(config->task_stack_size, config->task_priority, this, s_task),
        uhci_ctrl(nullptr), rx_buffer(nullptr), rx_buffer_size(0),
        use_dma(config->uart_config.use_dma), dma_buffer_size(config->uart_config.dma_buffer_size),
        rx_semaphore(nullptr), received_size(0), rx_complete(false)
    {
        if (use_dma) {
            initialize_uhci();
        }
    }

    ~UartDmaTerminal() override
    {
        if (uhci_ctrl) {
            uhci_del_controller(uhci_ctrl);
        }
        if (rx_buffer) {
            free(rx_buffer);
        }
        if (rx_semaphore) {
            vSemaphoreDelete(rx_semaphore);
        }
        if (rx_mutex) {
            vSemaphoreDelete(rx_mutex);
        }
    }

    void start() override
    {
        signal.set(TASK_START);
    }

    void stop() override
    {
        signal.set(TASK_STOP);
    }

    int write(uint8_t *data, size_t len) override;

    int read(uint8_t *data, size_t len) override;

    void set_read_cb(std::function<bool(uint8_t *data, size_t len)> f) override
    {
        on_read = std::move(f);
    }

private:
    void initialize_uhci();
    static bool uhci_rx_event_callback(uhci_controller_handle_t uhci_ctrl, const uhci_rx_event_data_t *edata, void *user_ctx);
    static bool uhci_tx_done_callback(uhci_controller_handle_t uhci_ctrl, const uhci_tx_done_event_data_t *edata, void *user_ctx);

    static void s_task(void *task_param)
    {
        auto t = static_cast<UartDmaTerminal *>(task_param);
        t->task();
        t->task_handle.task_handle = nullptr;
        vTaskDelete(nullptr);
    }

    void task();
    bool get_event(uart_event_t &event, uint32_t time_ms)
    {
        return xQueueReceive(event_queue, &event, pdMS_TO_TICKS(time_ms));
    }

    void reset_events()
    {
        uart_flush_input(uart.port);
        xQueueReset(event_queue);
    }

    static const size_t TASK_INIT = BIT0;
    static const size_t TASK_START = BIT1;
    static const size_t TASK_STOP = BIT2;

    QueueHandle_t event_queue;
    uart_resource uart;
    SignalGroup signal;
    uart_dma_task task_handle;

    // UHCI DMA specific members
    uhci_controller_handle_t uhci_ctrl;
    uint8_t *rx_buffer;
    size_t rx_buffer_size;
    bool use_dma;
    size_t dma_buffer_size;
    SemaphoreHandle_t rx_semaphore;
    SemaphoreHandle_t rx_mutex;  // Mutex for protecting shared DMA state
    size_t received_size;
    bool rx_complete;
};

void UartDmaTerminal::initialize_uhci()
{
    uhci_controller_config_t uhci_cfg = {
        .uart_port = uart.port,
        .tx_trans_queue_depth = 2,
        .max_receive_internal_mem = dma_buffer_size,
        .max_transmit_size = dma_buffer_size,
        .dma_burst_size = 32,
        .max_packet_receive = 0, // No limit
        .rx_eof_flags = {
            .rx_brk_eof = 0,
            .idle_eof = 1, // End on idle
            .length_eof = 0
        }
    };

    esp_err_t ret = uhci_new_controller(&uhci_cfg, &uhci_ctrl);
    ESP_MODEM_THROW_IF_ERROR(ret, "Failed to create UHCI controller");

    // Allocate DMA buffer
    rx_buffer = (uint8_t*)heap_caps_calloc(1, dma_buffer_size, MALLOC_CAP_DMA);
    ESP_MODEM_THROW_IF_FALSE(rx_buffer != nullptr, "Failed to allocate DMA buffer");
    rx_buffer_size = dma_buffer_size;

    // Create semaphore for RX synchronization
    rx_semaphore = xSemaphoreCreateBinary();
    ESP_MODEM_THROW_IF_FALSE(rx_semaphore != nullptr, "Failed to create RX semaphore");

    // Create mutex for protecting shared DMA state
    rx_mutex = xSemaphoreCreateMutex();
    ESP_MODEM_THROW_IF_FALSE(rx_mutex != nullptr, "Failed to create RX mutex");

    // Register callbacks
    uhci_event_callbacks_t uhci_cbs = {
        .on_rx_trans_event = uhci_rx_event_callback,
        .on_tx_trans_done = uhci_tx_done_callback,
    };

    ret = uhci_register_event_callbacks(uhci_ctrl, &uhci_cbs, this);
    ESP_MODEM_THROW_IF_ERROR(ret, "Failed to register UHCI callbacks");

    ESP_LOGI(TAG, "UHCI DMA initialized with buffer size: %zu", dma_buffer_size);
}

bool UartDmaTerminal::uhci_rx_event_callback(uhci_controller_handle_t uhci_ctrl, const uhci_rx_event_data_t *edata, void *user_ctx)
{
    UartDmaTerminal *terminal = static_cast<UartDmaTerminal*>(user_ctx);

    // Protect shared state with mutex (ISR-safe)
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (xSemaphoreTakeFromISR(terminal->rx_mutex, &xHigherPriorityTaskWoken) == pdTRUE) {
        terminal->received_size = edata->recv_size;
        terminal->rx_complete = edata->flags.totally_received;
        xSemaphoreGiveFromISR(terminal->rx_mutex, &xHigherPriorityTaskWoken);
    }

    // Notify the task that data is available
    xSemaphoreGiveFromISR(terminal->rx_semaphore, &xHigherPriorityTaskWoken);

    return xHigherPriorityTaskWoken; // Return whether a higher priority task was woken
}

bool UartDmaTerminal::uhci_tx_done_callback(uhci_controller_handle_t uhci_ctrl, const uhci_tx_done_event_data_t *edata, void *user_ctx)
{
    // TX completion can be handled here if needed
    return false;
}

std::unique_ptr<Terminal> create_uart_dma_terminal(const esp_modem_dte_config *config)
{
    TRY_CATCH_RET_NULL(
        auto term = std::make_unique<UartDmaTerminal>(config);
        term->start();
        return term;
    )
}

void UartDmaTerminal::task()
{
    uart_event_t event;
    size_t len;
    signal.set(TASK_INIT);
    signal.wait_any(TASK_START | TASK_STOP, portMAX_DELAY);
    if (signal.is_any(TASK_STOP)) {
        return; // exits to the static method where the task gets deleted
    }

    if (use_dma) {
        // Start DMA receive
        esp_err_t ret = uhci_receive(uhci_ctrl, rx_buffer, rx_buffer_size);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start UHCI receive: %s", esp_err_to_name(ret));
        }
    }

    while (signal.is_any(TASK_START)) {
        if (use_dma) {
            // Wait for DMA data or timeout
            if (xSemaphoreTake(rx_semaphore, pdMS_TO_TICKS(100)) == pdTRUE) {
                // Protect shared state access
                if (xSemaphoreTake(rx_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    size_t current_received_size = received_size;
                    bool current_rx_complete = rx_complete;
                    xSemaphoreGive(rx_mutex);

                    if (on_read && current_received_size > 0) {
                        on_read(rx_buffer, current_received_size);
                    }

                    // Always restart receive for continuous operation
                    // The UHCI driver will handle the case where receive is already active
                    esp_err_t ret = uhci_receive(uhci_ctrl, rx_buffer, rx_buffer_size);
                    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
                        ESP_LOGE(TAG, "Failed to restart UHCI receive: %s", esp_err_to_name(ret));
                    }
                }
            }
        } else {
            // Traditional UART event handling
            if (get_event(event, 100)) {
                switch (event.type) {
                case UART_DATA:
                    uart_get_buffered_data_len(uart.port, &len);
                    if (len && on_read) {
                        on_read(nullptr, len);
                    }
                    break;
                case UART_FIFO_OVF:
                    ESP_LOGW(TAG, "HW FIFO Overflow");
                    if (on_error) {
                        on_error(terminal_error::BUFFER_OVERFLOW);
                    }
                    reset_events();
                    break;
                case UART_BUFFER_FULL:
                    ESP_LOGW(TAG, "Ring Buffer Full");
                    if (on_error) {
                        on_error(terminal_error::BUFFER_OVERFLOW);
                    }
                    reset_events();
                    break;
                case UART_BREAK:
                    ESP_LOGW(TAG, "Rx Break");
                    if (on_error) {
                        on_error(terminal_error::UNEXPECTED_CONTROL_FLOW);
                    }
                    break;
                case UART_PARITY_ERR:
                    ESP_LOGE(TAG, "Parity Error");
                    if (on_error) {
                        on_error(terminal_error::CHECKSUM_ERROR);
                    }
                    break;
                case UART_FRAME_ERR:
                    ESP_LOGE(TAG, "Frame Error");
                    if (on_error) {
                        on_error(terminal_error::UNEXPECTED_CONTROL_FLOW);
                    }
                    break;
                default:
                    ESP_LOGW(TAG, "unknown uart event type: %d", event.type);
                    break;
                }
            } else {
                uart_get_buffered_data_len(uart.port, &len);
                if (len && on_read) {
                    on_read(nullptr, len);
                }
            }
        }
    }
}

int UartDmaTerminal::read(uint8_t *data, size_t len)
{
    if (use_dma) {
        // Protect shared state access
        if (xSemaphoreTake(rx_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            // For DMA mode, data is already in rx_buffer
            // Copy the requested amount
            size_t copy_len = std::min(len, received_size);
            if (copy_len > 0) {
                memcpy(data, rx_buffer, copy_len);
                // Move remaining data to beginning of buffer
                if (received_size > copy_len) {
                    memmove(rx_buffer, rx_buffer + copy_len, received_size - copy_len);
                    received_size -= copy_len;
                } else {
                    received_size = 0;
                }
            }
            xSemaphoreGive(rx_mutex);
            return copy_len;
        }
        return 0; // Timeout or error
    } else {
        // Traditional UART read
        size_t length = 0;
        uart_get_buffered_data_len(uart.port, &length);
        length = std::min(len, length);
        if (length > 0) {
            int read_len = uart_read_bytes(uart.port, data, length, portMAX_DELAY);
#if CONFIG_ESP_MODEM_ADD_DEBUG_LOGS
            ESP_LOG_BUFFER_HEXDUMP("uart-rx", data, read_len, ESP_LOG_DEBUG);
#endif
            return read_len;
        }
        return 0;
    }
}

int UartDmaTerminal::write(uint8_t *data, size_t len)
{
    if (use_dma) {
        // Use UHCI for DMA transmission
        esp_err_t ret = uhci_transmit(uhci_ctrl, data, len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "UHCI transmit failed: %s", esp_err_to_name(ret));
            return -1;
        }
#if CONFIG_ESP_MODEM_ADD_DEBUG_LOGS
        ESP_LOG_BUFFER_HEXDUMP("uart-tx", data, len, ESP_LOG_DEBUG);
#endif
        return len;
    } else {
        // Traditional UART write
#if CONFIG_ESP_MODEM_ADD_DEBUG_LOGS
        ESP_LOG_BUFFER_HEXDUMP("uart-tx", data, len, ESP_LOG_DEBUG);
#endif
        return uart_write_bytes_compat(uart.port, data, len);
    }
}

} // namespace esp_modem
