/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <optional>
#include <unistd.h>
#include "cxx_include/esp_modem_dte.hpp"
#include "esp_log.h"
#include "esp_modem_config.h"
#include "exception_stub.hpp"

static const char *TAG = "fs_terminal";

namespace esp_modem {


struct File {
    explicit File(const esp_modem_dte_config *config):
        fd(config->vfs_config.fd), deleter(config->vfs_config.deleter), resource(config->vfs_config.resource)
    {}

    ~File()
    {
        if (deleter) {
            deleter(fd, resource);
        }
    }
    int fd;
    void (*deleter)(int fd, struct esp_modem_vfs_resource *res);
    struct esp_modem_vfs_resource *resource;
};

class FdTerminal : public Terminal {
public:
    explicit FdTerminal(const esp_modem_dte_config *config);

    ~FdTerminal() override;

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
        // Block until the task confirms it has left the processing loop, so no read callback is
        // in flight when the task is deleted .
        if (!signal.wait_any(TASK_STOPPED, stop_timeout_ms)) {
            ESP_LOGW(TAG, "Timed out waiting for fs task to stop");
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
    void task();

    // Invoke on_read under cb_lock so it cannot be reassigned/cleared (set_mode()/teardown on
    // another task) while we're executing it; re-check under the lock.
    void notify_read()
    {
        Scoped<Lock> l(cb_lock);
        if (on_read) {
            on_read(nullptr, 0);
        }
    }

    static const size_t TASK_INIT = SignalGroup::bit0;
    static const size_t TASK_START = SignalGroup::bit1;
    static const size_t TASK_STOP = SignalGroup::bit2;
    static const size_t TASK_STOPPED = SignalGroup::bit3;   /*!< Set by the task once it has left the processing loop */
    static const uint32_t stop_timeout_ms = 2000;           /*!< Max wait for a graceful stop (must exceed the select() timeout) */

    File f;
    SignalGroup signal;
    Task task_handle;
};

std::unique_ptr<Terminal> create_vfs_terminal(const esp_modem_dte_config *config)
{
    TRY_CATCH_RET_NULL(
        auto term = std::make_unique<FdTerminal>(config);
        term->start();
        return term;
    )
}

FdTerminal::FdTerminal(const esp_modem_dte_config *config) :
    f(config), signal(),
    task_handle(config->task_stack_size, config->task_priority, this, [](void *p)
{
    auto t = static_cast<FdTerminal *>(p);
    t->task();
    // task() returns only after stop() requested it. Acknowledge that the processing loop has
    // been left -- this is the last access to any member.
    t->signal.set(TASK_STOPPED);
#if !defined(CONFIG_IDF_TARGET_LINUX)
    // FreeRTOS: a task function must not return. Idle until the owner (Task destructor) deletes
    // us; we deliberately do not self-delete, to avoid racing that delete.
    while (true) {
        Task::Delay(3600 * 1000);
    }
#endif
    // Linux: returning ends the std::thread, which the Task destructor join()s.
})
{}

void FdTerminal::task()
{
    signal.set(TASK_INIT);
    signal.wait_any(TASK_START | TASK_STOP, portMAX_DELAY);
    if (signal.is_any(TASK_STOP)) {
        return; // exits to the static method where the task gets deleted
    }

    while (signal.is_any(TASK_START)) {
        int s;
        fd_set rfds;
        struct timeval tv = {
            .tv_sec = 1,
            .tv_usec = 0,
        };
        FD_ZERO(&rfds);
        FD_SET(f.fd, &rfds);

        s = select(f.fd + 1, &rfds, nullptr, nullptr, &tv);

        if (s < 0) {
            break;
        } else if (s == 0) {
//            ESP_LOGV(TAG, "Select exited with timeout");
        } else {
            if (FD_ISSET(f.fd, &rfds)) {
                notify_read();
            }
        }
        Task::Relinquish();
    }
}

int FdTerminal::read(uint8_t *data, size_t len)
{
    int size = ::read(f.fd, data, len);
    if (size < 0) {
        if (errno != EAGAIN) {
            ESP_LOGE(TAG, "Error occurred during read: %d", errno);
        }
        return 0;
    }

    return size;
}

int FdTerminal::write(uint8_t *data, size_t len)
{
    int size = ::write(f.fd, data, len);
    if (size < 0) {
        ESP_LOGE(TAG, "Error occurred during read: %d", errno);
        return 0;
    }
    return size;
}

FdTerminal::~FdTerminal()
{
    FdTerminal::stop();
}

} // namespace esp_modem
