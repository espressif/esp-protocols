// Copyright 2021 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _ESP_MODEM_PRIMITIVES_HPP_
#define _ESP_MODEM_PRIMITIVES_HPP_
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

namespace esp_modem {

#ifdef CONFIG_COMPILER_CXX_EXCEPTIONS
#define THROW(exception) throw(exception)
class esp_err_exception: virtual public std::exception {
public:
    explicit esp_err_exception(esp_err_t err): esp_err(err) {}
    explicit esp_err_exception(std::string msg): esp_err(ESP_FAIL), message(std::move(msg)) {}
    explicit esp_err_exception(std::string msg, esp_err_t err): esp_err(err), message(std::move(msg)) {}
    virtual esp_err_t get_err_t() { return esp_err; }
    ~esp_err_exception() noexcept override = default;
    virtual const char* what() const noexcept {
        return message.c_str();
    }
private:
    esp_err_t esp_err;
    std::string message;
};
#else
#define THROW(exception) abort()
#endif

static inline void throw_if_false(bool condition, std::string message)
{
    if (!condition) {
        THROW(esp_err_exception(std::move(message)));
    }
}

static inline void throw_if_esp_fail(esp_err_t err, std::string message)
{
    if (err != ESP_OK) {
        THROW(esp_err_exception(std::move(message), err));
    }
}

static inline void throw_if_esp_fail(esp_err_t err)
{
    if (err != ESP_OK) {
        THROW(esp_err_exception(err));
    }
}

struct Lock {
    explicit Lock(): lock(nullptr)
    {
        lock = xSemaphoreCreateRecursiveMutex();
        throw_if_false(lock != nullptr, "create signal event group failed");
    }
    ~Lock() { vSemaphoreDelete(lock); }
    void take() { xSemaphoreTakeRecursive(lock, portMAX_DELAY); }

    void give() { xSemaphoreGiveRecursive(lock); }
    xSemaphoreHandle lock;
};

template<class T>
class Scoped {
public:
    explicit Scoped(T &l):lock(l) { lock.take(); }
    ~Scoped() { lock.give(); }

private:
    T& lock;
};

struct signal_group {
    explicit signal_group(): event_group(nullptr)
    {
        event_group = xEventGroupCreate();
        throw_if_false(event_group != nullptr, "create signal event group failed");
    }

    void set(uint32_t bits)
    {
        xEventGroupSetBits(event_group, bits);
    }

    void clear(uint32_t bits)
    {
        xEventGroupClearBits(event_group, bits);
    }

    bool wait(uint32_t flags, uint32_t time_ms) // waiting for all and clearing if set
    {
        EventBits_t bits = xEventGroupWaitBits(event_group, flags, pdTRUE, pdTRUE, pdMS_TO_TICKS(time_ms));
        return bits & flags;
    }

    bool is_any(uint32_t flags)
    {
        return xEventGroupGetBits(event_group) & flags;
    }

    bool wait_any(uint32_t flags, uint32_t time_ms) // waiting for any bit, not clearing them
    {
        EventBits_t bits = xEventGroupWaitBits(event_group, flags, pdFALSE, pdFALSE, pdMS_TO_TICKS(time_ms));
        return bits & flags;
    }

    ~signal_group()
    {
        if (event_group) vEventGroupDelete(event_group);
    }

    EventGroupHandle_t event_group;
};

} // namespace esp_modem

#endif // _ESP_MODEM_PRIMITIVES_HPP_
