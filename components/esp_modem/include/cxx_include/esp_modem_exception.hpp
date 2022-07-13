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

#pragma once

#include <string>
#include "esp_err.h"
#include "esp_log.h"

#ifndef __FILENAME__
#define __FILENAME__    __FILE__
#endif
#define ESP_MODEM_THROW_IF_FALSE(...) esp_modem::throw_if_false(__FILENAME__, __LINE__, __VA_ARGS__)
#define ESP_MODEM_THROW_IF_ERROR(...) esp_modem::throw_if_error(__FILENAME__, __LINE__, __VA_ARGS__)

namespace esp_modem {

#ifdef CONFIG_COMPILER_CXX_EXCEPTIONS
#define ESP_MODEM_THROW(exception) throw(exception)

class esp_err_exception: virtual public std::exception {
public:
    explicit esp_err_exception(std::string msg): esp_err(ESP_FAIL), message(std::move(msg)) {}
    explicit esp_err_exception(std::string msg, esp_err_t err): esp_err(err), message(std::move(msg)) {}
    virtual esp_err_t get_err_t()
    {
        return esp_err;
    }
    ~esp_err_exception() noexcept override = default;
    [[nodiscard]] const char *what() const noexcept override
    {
        return message.c_str();
    }
private:
    esp_err_t esp_err;
    std::string message;
};

#else
#define ESP_MODEM_THROW(exception) do { exception; abort(); } while(0)

class esp_err_exception {
    void print(std::string msg) { ESP_LOGE("ESP_MODEM_THROW", "%s\n", msg.c_str()); }
public:
    explicit esp_err_exception(std::string msg) { print(std::move(msg)); }
    explicit esp_err_exception(std::string msg, esp_err_t err) { print(std::move(msg)); }
};

#endif

static inline std::string make_message(const std::string& filename, int line, const std::string& message = "ERROR")
{
    std::string text = filename + ":" + std::to_string(line) + " " + message;
    return text;
}

static inline void throw_if_false(const std::string& filename, int line, bool condition, const std::string& message)
{
    if (!condition) {
        ESP_MODEM_THROW(esp_err_exception(make_message(filename, line, message)));
    }
}

static inline void throw_if_error(const std::string& filename, int line, esp_err_t err, const std::string& message)
{
    if (err != ESP_OK) {
        ESP_MODEM_THROW(esp_err_exception(make_message(filename, line, message), err));
    }
}

static inline void throw_if_error(const std::string& filename, int line, esp_err_t err)
{
    if (err != ESP_OK) {
        ESP_MODEM_THROW(esp_err_exception(make_message(filename, line), err));
    }
}

} // namespace esp_modem
