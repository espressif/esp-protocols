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

namespace esp_modem {

#ifdef CONFIG_COMPILER_CXX_EXCEPTIONS
#define THROW(exception) throw(exception)
class esp_err_exception: virtual public std::exception {
public:
    explicit esp_err_exception(esp_err_t err): esp_err(err) {}
    explicit esp_err_exception(std::string msg): esp_err(ESP_FAIL), message(std::move(msg)) {}
    explicit esp_err_exception(std::string msg, esp_err_t err): esp_err(err), message(std::move(msg)) {}
    virtual esp_err_t get_err_t()
    {
        return esp_err;
    }
    ~esp_err_exception() noexcept override = default;
    virtual const char *what() const noexcept
    {
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

} // namespace esp_modem
