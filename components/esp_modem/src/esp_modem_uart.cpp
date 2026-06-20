/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "uart_compat.h"
#include "esp_modem_config.h"
#include "exception_stub.hpp"
#include "cxx_include/esp_modem_dte.hpp"
#include "uart_resource.hpp"

static const char *TAG = "uart_terminal";

namespace esp_modem {


struct uart_task {
    explicit uart_task(size_t stack_size, size_t priority, void *task_param, TaskFunction_t task_function) :
        task_handle(nullptr)
    {
        BaseType_t ret = xTaskCreate(task_function, "uart_task", stack_size, task_param, priority, &task_handle);
        ESP_MODEM_THROW_IF_FALSE(ret == pdTRUE, "create uart event task failed");
    }

    ~uart_task()
    {
        if (task_handle) {
            vTaskDelete(task_handle);
        }
    }

    TaskHandle_t task_handle;       /*!< UART event task handle */
};



class UartTerminal : public Terminal {
public:
    explicit UartTerminal(const esp_modem_dte_config *config) :
        event_queue(), uart(&config->uart_config, &event_queue, -1), signal(),
        task_handle(config->task_stack_size, config->task_priority, this, s_task) {}

    ~UartTerminal() override
    {
        // Stop the RX task gracefully (and synchronously) before our members are torn down,
        // so it is never force-deleted while inside a callback or blocked in the UART driver.
        UartTerminal::stop();
    }

    void start() override
    {
        signal.set(TASK_START);
    }

    void stop() override
    {
        if (signal.is_any(TASK_STOPPED)) {
            return;     // already stopped (stop() is also called from the destructor)
        }
        signal.clear(TASK_START);   // make the processing loop exit
        signal.set(TASK_STOP);      // release the task if it never started
        // Block until the task confirms it has left the processing loop: no read/error callback
        // is in flight, and it is not blocked in the UART driver, so it is safe to delete.
        if (!signal.wait_any(TASK_STOPPED, stop_timeout_ms)) {
            ESP_LOGW(TAG, "Timed out waiting for uart_task to stop");
        }
    }

    int write(uint8_t *data, size_t len) override;

    int read(uint8_t *data, size_t len) override;

    void set_read_cb(std::function<bool(uint8_t *data, size_t len)> f) override
    {
        Scoped<Lock> l(cb_lock);
        on_read = std::move(f);
    }

private:
    static void s_task(void *task_param)
    {
        auto t = static_cast<UartTerminal *>(task_param);
        t->task();
        // task() returns only after stop() requested it. Acknowledge that the processing loop has
        // been left -- this is the last access to any member -- then idle until the owner deletes
        // us from ~uart_task(). We deliberately do not self-delete, to avoid racing that delete.
        t->signal.set(TASK_STOPPED);
        while (true) {
            vTaskDelay(portMAX_DELAY);
        }
    }

    void task();
    bool get_event(uart_event_t &event, uint32_t time_ms)
    {
        return xQueueReceive(event_queue, &event, pdMS_TO_TICKS(time_ms));
    }

    // Invoke the read/error callbacks under cb_lock so they cannot be reassigned or cleared
    // (by set_mode()/teardown on another task) while we're executing them. Re-check under the
    // lock, since the callback may have been cleared between the outer check and acquiring it.
    void notify_read(size_t len)
    {
        Scoped<Lock> l(cb_lock);
        if (on_read) {
            on_read(nullptr, len);
        }
    }
    void notify_error(terminal_error err)
    {
        Scoped<Lock> l(cb_lock);
        if (on_error) {
            on_error(err);
        }
    }

    void reset_events()
    {
        uart_flush_input(uart.port);
        xQueueReset(event_queue);
    }

    static const size_t TASK_INIT = BIT0;
    static const size_t TASK_START = BIT1;
    static const size_t TASK_STOP = BIT2;
    static const size_t TASK_STOPPED = BIT3;        /*!< Set by the task once it has left the processing loop */
    static const uint32_t stop_timeout_ms = 1000;   /*!< Max wait for a graceful stop before forcing deletion */

    QueueHandle_t event_queue;
    uart_resource uart;
    SignalGroup signal;
    uart_task task_handle;
};

std::unique_ptr<Terminal> create_uart_terminal(const esp_modem_dte_config *config)
{
    TRY_CATCH_RET_NULL(
        auto term = std::make_unique<UartTerminal>(config);
        term->start();
        return term;
    )
}

void UartTerminal::task()
{
    uart_event_t event;
    size_t len;
    signal.set(TASK_INIT);
    signal.wait_any(TASK_START | TASK_STOP, portMAX_DELAY);
    if (signal.is_any(TASK_STOP)) {
        return; // exits to the static method where the task gets deleted
    }
    while (signal.is_any(TASK_START)) {
        if (get_event(event, 100)) {
            switch (event.type) {
            case UART_DATA:
                uart_get_buffered_data_len(uart.port, &len);
                if (len) {
                    notify_read(len);
                }
                break;
            case UART_FIFO_OVF:
                ESP_LOGW(TAG, "HW FIFO Overflow");
                notify_error(terminal_error::BUFFER_OVERFLOW);
                reset_events();
                break;
            case UART_BUFFER_FULL:
                ESP_LOGW(TAG, "Ring Buffer Full");
                notify_error(terminal_error::BUFFER_OVERFLOW);
                reset_events();
                break;
            case UART_BREAK:
                ESP_LOGW(TAG, "Rx Break");
                notify_error(terminal_error::UNEXPECTED_CONTROL_FLOW);
                break;
            case UART_PARITY_ERR:
                ESP_LOGE(TAG, "Parity Error");
                notify_error(terminal_error::CHECKSUM_ERROR);
                break;
            case UART_FRAME_ERR:
                ESP_LOGE(TAG, "Frame Error");
                notify_error(terminal_error::UNEXPECTED_CONTROL_FLOW);
                break;
            default:
                ESP_LOGW(TAG, "unknown uart event type: %d", event.type);
                break;
            }
        } else {
            uart_get_buffered_data_len(uart.port, &len);
            if (len) {
                notify_read(len);
            }
        }
    }
}

int UartTerminal::read(uint8_t *data, size_t len)
{
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

int UartTerminal::write(uint8_t *data, size_t len)
{
#if CONFIG_ESP_MODEM_ADD_DEBUG_LOGS
    ESP_LOG_BUFFER_HEXDUMP("uart-tx", data, len, ESP_LOG_DEBUG);
#endif
    return uart_write_bytes_compat(uart.port, data, len);
}

} // namespace esp_modem
