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

#if GENERATE_DOCS
#define _ARG(param, name) name
#else
#define _ARG(param, name) param
#endif

#ifdef __cplusplus
#include <string>
#define STRING_IN(param, name) const std::string& _ARG(param, name)
#define STRING_OUT(param, name) std::string& _ARG(param, name)
#define BOOL_IN(param, name) const bool  _ARG(param, name)
#define BOOL_OUT(param, name) bool&  _ARG(param, name)
#define INT_OUT(param, name) int&  _ARG(param, name)

#define STRUCT_OUT(struct_name, x)  struct_name& x
#else
#define STRING_IN(param, name) const char* _ARG(param, name)
#define STRING_OUT(param, name) char* _ARG(param, name)
#define BOOL_IN(param, name) const bool _ARG(param, name)
#define BOOL_OUT(param, name) bool* _ARG(param, name)
#define INT_OUT(param, name) int* _ARG(param, name)
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
 *
 */ \
    \
ESP_MODEM_DECLARE_DCE_COMMAND(set_pin,  command_result, 1, MUX_ARG, STRING_IN(x, pin)) \
\
\
/**
 * @brief Checks if the SIM needs a PIN
 *
 * @param[out] pin_ok Pin
 */ \
ESP_MODEM_DECLARE_DCE_COMMAND(read_pin, command_result, 1, MUX_ARG, BOOL_OUT(x, pin_ok))           \
    \
/**
 * @brief Reads the module name
 *
 * @param[out] name module name
 */ \
ESP_MODEM_DECLARE_DCE_COMMAND(set_echo, command_result, 1, MUX_ARG, BOOL_IN(x, echo_on))           \
ESP_MODEM_DECLARE_DCE_COMMAND(sms_txt_mode, command_result, 1, MUX_ARG, BOOL_IN(x, txt))           \
ESP_MODEM_DECLARE_DCE_COMMAND(sms_character_set, command_result, 0, MUX_ARG)           \
ESP_MODEM_DECLARE_DCE_COMMAND(send_sms, command_result, 2, MUX_ARG, STRING_IN(x, number), STRING_IN(y, message))           \
                                      \
    \
/**
 * @brief Reads the module name
 *
 * @param[out] name module name
 */ \
ESP_MODEM_DECLARE_DCE_COMMAND(resume_data_mode, command_result, 0, MUX_ARG)           \
    \
/**
 * @brief Reads the module name
 *
 * @param[out] name module name
 */ \
ESP_MODEM_DECLARE_DCE_COMMAND(set_pdp_context, command_result, 1, MUX_ARG, STRUCT_OUT(PdpContext, x)) \
    \
/**
 * @brief Reads the module name
 *
 * @param[out] name module name
 */ \
ESP_MODEM_DECLARE_DCE_COMMAND(set_command_mode, command_result, 0, MUX_ARG) \
    \
/**
 * @brief Reads the module name
 *
 * @param[out] name module name
 */ \
ESP_MODEM_DECLARE_DCE_COMMAND(set_cmux, command_result, 0, MUX_ARG) \
    \
/**
 * @brief Reads the module name
 *
 * @param[out] name module name
 */ \
ESP_MODEM_DECLARE_DCE_COMMAND(get_imsi, command_result, 1, MUX_ARG, STRING_OUT(x, imsi)) \
    \
/**
 * @brief Reads the module name
 *
 * @param[out] name module name
 */ \
ESP_MODEM_DECLARE_DCE_COMMAND(get_imei, command_result, 1, MUX_ARG, STRING_OUT(x, name)) \
    \
/**
 * @brief Reads the module name
 *
 * @param[out] name module name
 */ \
ESP_MODEM_DECLARE_DCE_COMMAND(get_module_name, command_result, 1, MUX_ARG, STRING_OUT(x, name)) \
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
ESP_MODEM_DECLARE_DCE_COMMAND(get_signal_quality, command_result, 2, MUX_ARG, INT_OUT(x, rssi), INT_OUT(y, ber))

#ifdef GENERATE_DOCS
// cat ../include/generate/esp_modem_command_declare.inc | clang++ -E -P -CC  -xc++ -I../include -DGENERATE_DOCS  - | sed -n '1,/DCE command documentation/!p'
// cat ../include/generate/esp_modem_command_declare.inc | clang -E -P -CC  -xc -I../include -DGENERATE_DOCS  - | sed -n '1,/DCE command documentation/!p' > c_api.h
// cat ../include/generate/esp_modem_command_declare.inc | clang -E -P  -xc -I../include -DGENERATE_DOCS -DGENERATE_RST_LINKS - | sed 's/NL/\n/g' > cxx_api_links.rst
//  --- DCE command documentation starts here ---
#ifdef __cplusplus
class esp_modem::DCE: public DCE_T<GenericModule> {
public:
    using DCE_T<GenericModule>::DCE_T;
#define ESP_MODEM_DECLARE_DCE_COMMAND(name, return_type, TEMPLATE_ARG, MUX_ARG, ...) return_type name (__VA_ARGS__);
#elif defined(GENERATE_RST_LINKS)
#define ESP_MODEM_DECLARE_DCE_COMMAND(name, return_type, TEMPLATE_ARG, MUX_ARG, ...) NL- :cpp:func:`esp_modem::DCE::name`
#else
#define ESP_MODEM_DECLARE_DCE_COMMAND(name, return_type, TEMPLATE_ARG, MUX_ARG, ...) return_type esp_modem_ ## name (__VA_ARGS__);
#endif

DECLARE_ALL_COMMAND_APIS()

#ifdef __cplusplus
};
#endif

#endif

#endif // _ESP_MODEM_COMMAND_DECLARE_INC_
