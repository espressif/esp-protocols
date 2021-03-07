//
// Created by david on 3/3/21.
//

#ifndef SIMPLE_CXX_CLIENT_ESP_MODEM_TERMINAL_HPP
#define SIMPLE_CXX_CLIENT_ESP_MODEM_TERMINAL_HPP


#include <memory>
#include <functional>
#include <exception>
#include <cstddef>
#include <cstdint>
#include <utility>
#include "esp_err.h"
#include "esp_modem_primitives.hpp"


enum class terminal_error {
    BUFFER_OVERFLOW,
    CHECKSUM_ERROR,
    UNEXPECTED_CONTROL_FLOW,
};

class Terminal {
public:
    virtual ~Terminal() = default;
    virtual void set_data_cb(std::function<bool(size_t len)> f) { on_data = std::move(f); }
    void set_error_cb(std::function<void(terminal_error)> f) { on_error = std::move(f); }
    virtual int write(uint8_t *data, size_t len) = 0;
    virtual int read(uint8_t *data, size_t len) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual size_t max_virtual_terms() { return 1; }

protected:
    std::function<bool(size_t len)> on_data;
    std::function<void(terminal_error)> on_error;
};



#endif //SIMPLE_CXX_CLIENT_ESP_MODEM_TERMINAL_HPP
