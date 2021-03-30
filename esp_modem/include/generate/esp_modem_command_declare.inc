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

#ifndef _ESP_MODEM_COMMAND_DECLARE_INC_
#define _ESP_MODEM_COMMAND_DECLARE_INC_

#ifdef __cplusplus
#include <string>
#define STRING_IN(x) const std::string& x
#define STRING_OUT(x) std::string& x
#define BOOL_IN(x) const bool x
#define BOOL_OUT(x) bool& x
#define INT_OUT(x) int& x

#define STRUCT_OUT(struct_name, x)  struct_name& x
#else
#define STRING_IN(x) const char* x
#define STRING_OUT(x) char* x
#define BOOL_IN(x) const bool x
#define BOOL_OUT(x) bool* x
#define INT_OUT(x) int* x
#define STRUCT_OUT(struct_name, x)  struct struct_name* x
#endif

#define _ESP_MODEM_COMMAND_DECLARE_INC_
#define ESP_MODEM_DEFINE_DCE_COMMAND(...) ESP_MODEM_DECLARE_DCE_COMMAND(##__VA_ARGS__)
#define DEFINE_ALL_COMMAND_APIS() DECLARE_ALL_COMMAND_APIS()

#define DECLARE_ALL_COMMAND_APIS(...) \
/**
 * @brief Sends the supplied PIN code
 *
 * @param pin Pin
 */ \
    \
ESP_MODEM_DECLARE_DCE_COMMAND(set_pin,  command_result, 1, MUX_ARG, STRING_IN(x)) \
/**
 * @brief Checks if the SIM needs a PIN
 *
 * @param[out] pin_ok Pin
 */ \
ESP_MODEM_DECLARE_DCE_COMMAND(read_pin, command_result, 1, MUX_ARG, BOOL_OUT(x))           \
    \
ESP_MODEM_DECLARE_DCE_COMMAND(set_echo, command_result, 1, MUX_ARG, BOOL_IN(x))           \
    \
ESP_MODEM_DECLARE_DCE_COMMAND(resume_data_mode, command_result, 0, MUX_ARG)           \
    \
ESP_MODEM_DECLARE_DCE_COMMAND(set_pdp_context, command_result, 1, MUX_ARG, STRUCT_OUT(PdpContext, x)) \
    \
ESP_MODEM_DECLARE_DCE_COMMAND(set_command_mode, command_result, 0, MUX_ARG) \
    \
ESP_MODEM_DECLARE_DCE_COMMAND(set_cmux, command_result, 0, MUX_ARG) \
    \
ESP_MODEM_DECLARE_DCE_COMMAND(get_imsi, command_result, 1, MUX_ARG, STRING_OUT(x)) \
    \
ESP_MODEM_DECLARE_DCE_COMMAND(get_imei, command_result, 1, MUX_ARG, STRING_OUT(x)) \
    \
/**
 * @brief Reads the module name
 *
 * @param[out]  module name
 */ \
ESP_MODEM_DECLARE_DCE_COMMAND(get_module_name, command_result, 1, MUX_ARG, STRING_OUT(x)) \
\
/**
 * @brief Sets the modem to data mode
 *
 */ \
ESP_MODEM_DECLARE_DCE_COMMAND(set_data_mode, command_result, 0, MUX_ARG) \
\
/**
 * @brief Get Signal quality
 *
 */ \
ESP_MODEM_DECLARE_DCE_COMMAND(get_signal_quality, command_result, 2, MUX_ARG, INT_OUT(x), INT_OUT(y))

//  --- DCE command documentation starts here ---
#ifdef GENERATE_DOCS
// gcc -E -CC -P -DGENERATE_DOCS esp_modem_command_declare.inc | sed -n '/DCE command documentation/,//p'

#define ESP_MODEM_DECLARE_DCE_COMMAND(name, return_type, TEMPLATE_ARG, MUX_ARG, ...) \
return_type name (__VA_ARGS__);

DECLARE_ALL_COMMAND_APIS()
#endif


#endif // _ESP_MODEM_COMMAND_DECLARE_INC_
