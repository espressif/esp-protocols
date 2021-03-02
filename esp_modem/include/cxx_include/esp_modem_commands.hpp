//
// Created by david on 3/2/21.
//

#ifndef SIMPLE_CXX_CLIENT_ESP_MODEM_COMMANDS_HPP
#define SIMPLE_CXX_CLIENT_ESP_MODEM_COMMANDS_HPP

#include "esp_err.h"

namespace esp_modem {
namespace dce_commands {

template <typename T> esp_err_t sync(T t);

} // dce_commands
} // esp_modem


#endif //SIMPLE_CXX_CLIENT_ESP_MODEM_COMMANDS_HPP
