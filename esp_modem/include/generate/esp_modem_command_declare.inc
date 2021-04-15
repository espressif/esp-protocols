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


// Parameters
// * handle different parameters for C++ and C API
// * make parameter unique names, so they could be easily referenced and forwarded
#define _ARG(param, name) param
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

#define DECLARE_ALL_COMMAND_APIS(...) \
/**
 * @brief Sets the supplied PIN code
 * @param[in] pin Pin
 * @return OK, FAIL or TIMEOUT
 */\
ESP_MODEM_DECLARE_DCE_COMMAND(set_pin,  command_result, 1, STRING_IN(x, pin)) \
\
/**
 * @brief Checks if the SIM needs a PIN
 * @param[out] pin_ok true if the SIM card doesn't need a PIN to unlock
 * @return OK, FAIL or TIMEOUT
 */ \
ESP_MODEM_DECLARE_DCE_COMMAND(read_pin, command_result, 1, BOOL_OUT(x, pin_ok)) \
    \
/**
 * @brief Sets echo mode
 * @param[in] echo_on true if echo mode on (repeats the commands)
 * @return OK, FAIL or TIMEOUT
 */ \
ESP_MODEM_DECLARE_DCE_COMMAND(set_echo, command_result, 1, BOOL_IN(x, echo_on)) \
    \
/**
 * @brief Sets the Txt or Pdu mode for SMS (only txt is supported)
 * @param[in] txt true if txt mode
 * @return OK, FAIL or TIMEOUT
 */ \
ESP_MODEM_DECLARE_DCE_COMMAND(sms_txt_mode, command_result, 1, BOOL_IN(x, txt)) \
    \
/**
 * @brief Sets the default (GSM) charater set
 * @return OK, FAIL or TIMEOUT
 */ \
ESP_MODEM_DECLARE_DCE_COMMAND(sms_character_set, command_result, 0) \
    \
/**
 * @brief Sends SMS message in txt mode
 * @param[in] number Phone number to send the message to
 * @param[in] message Text message to be sent
 * @return OK, FAIL or TIMEOUT
 */ \
ESP_MODEM_DECLARE_DCE_COMMAND(send_sms, command_result, 2, STRING_IN(x, number), STRING_IN(y, message)) \
    \
/**
 * @brief Resumes data mode (Switches back to th data mode, which was temporarily suspended)
 * @return OK, FAIL or TIMEOUT
 */ \
ESP_MODEM_DECLARE_DCE_COMMAND(resume_data_mode, command_result, 0) \
    \
/**
 * @brief Sets php context
 * @param[in] x PdP context struct to setup modem cellular connection
 * @return OK, FAIL or TIMEOUT
 */ \
ESP_MODEM_DECLARE_DCE_COMMAND(set_pdp_context, command_result, 1, STRUCT_OUT(PdpContext, x)) \
    \
/**
 * @brief Switches to the command mode
 * @return OK, FAIL or TIMEOUT
 */ \
ESP_MODEM_DECLARE_DCE_COMMAND(set_command_mode, command_result, 0) \
    \
/**
 * @brief Switches to the CMUX mode
 * @return OK, FAIL or TIMEOUT
 */ \
ESP_MODEM_DECLARE_DCE_COMMAND(set_cmux, command_result, 0) \
    \
/**
 * @brief Reads the IMSI number
 * @param[out] imsi Module's IMSI number
 * @return OK, FAIL or TIMEOUT
 */ \
ESP_MODEM_DECLARE_DCE_COMMAND(get_imsi, command_result, 1, STRING_OUT(x, imsi)) \
    \
/**
 * @brief Reads the IMEI number
 * @param[out] imei Module's IMEI number
 * @return OK, FAIL or TIMEOUT
 */ \
ESP_MODEM_DECLARE_DCE_COMMAND(get_imei, command_result, 1, STRING_OUT(x, imei)) \
    \
/**
 * @brief Reads the module name
 * @param[out] name module name
 * @return OK, FAIL or TIMEOUT
 */ \
ESP_MODEM_DECLARE_DCE_COMMAND(get_module_name, command_result, 1, STRING_OUT(x, name)) \
\
/**
 * @brief Sets the modem to data mode
 * @return OK, FAIL or TIMEOUT
 */ \
ESP_MODEM_DECLARE_DCE_COMMAND(set_data_mode, command_result, 0) \
\
/**
 * @brief Get Signal quality
 * @param[out] rssi signal strength indication
 * @param[out] ber channel bit error rate
 * @return OK, FAIL or TIMEOUT
 */ \
ESP_MODEM_DECLARE_DCE_COMMAND(get_signal_quality, command_result, 2, INT_OUT(x, rssi), INT_OUT(y, ber))

#ifdef GENERATE_DOCS
// cat ../include/generate/esp_modem_command_declare.inc | clang++ -E -P -CC  -xc++ -I../include -DGENERATE_DOCS  - | sed -n '1,/DCE command documentation/!p'
// cat ../include/generate/esp_modem_command_declare.inc | clang -E -P -CC  -xc -I../include -DGENERATE_DOCS  - | sed -n '1,/DCE command documentation/!p' > c_api.h
// cat ../include/generate/esp_modem_command_declare.inc | clang -E -P  -xc -I../include -DGENERATE_DOCS -DGENERATE_RST_LINKS - | sed 's/NL/\n/g' > cxx_api_links.rst

// call parametrs by names for documentation
#undef _ARG
#define _ARG(param, name) name
//  --- DCE command documentation starts here ---
#ifdef __cplusplus
class esp_modem::DCE: public DCE_T<GenericModule> {
public:
    using DCE_T<GenericModule>::DCE_T;
#define ESP_MODEM_DECLARE_DCE_COMMAND(name, return_type, TEMPLATE_ARG, ...) return_type name (__VA_ARGS__);
#elif defined(GENERATE_RST_LINKS)
#define ESP_MODEM_DECLARE_DCE_COMMAND(name, return_type, TEMPLATE_ARG, ...) NL- :cpp:func:`esp_modem::DCE::name`
#else
#define ESP_MODEM_DECLARE_DCE_COMMAND(name, return_type, TEMPLATE_ARG, ...) return_type esp_modem_ ## name (__VA_ARGS__);
#endif

DECLARE_ALL_COMMAND_APIS()

#ifdef __cplusplus
};
#endif

#endif

#endif // _ESP_MODEM_COMMAND_DECLARE_INC_
