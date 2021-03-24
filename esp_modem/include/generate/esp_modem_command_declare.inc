//
// Created by david on 3/7/21.
//

#ifndef SIMPLE_CXX_CLIENT_ESP_MODEM_COMMAND_DECLARE_INC
#define SIMPLE_CXX_CLIENT_ESP_MODEM_COMMAND_DECLARE_INC

#ifdef __cplusplus
#include <string>
#define STRING_IN(x) const std::string& x
#define STRING_OUT(x) std::string& x
#define BOOL_IN(x) const bool x
#define BOOL_OUT(x) bool& x
#define STRUCT_OUT(struct_name, x)  struct_name& x
#else
struct PdpContext;

#define STRING_IN(x) const char* x
#define STRING_OUT(x) char* x
#define BOOL_IN(x) const bool x
#define BOOL_OUT(x) bool* x
#define STRUCT_OUT(struct_name, x)  struct struct_name* x
#endif


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
ESP_MODEM_DECLARE_DCE_COMMAND(set_data_mode, command_result, 0, MUX_ARG)

//  --- DCE command documentation starts here ---
#ifdef GENERATE_DOCS
// gcc -E -CC -P -DGENERATE_DOCS esp_modem_command_declare.inc | sed -n '/DCE command documentation/,//p'

#define ESP_MODEM_DECLARE_DCE_COMMAND(name, return_type, TEMPLATE_ARG, MUX_ARG, ...) \
return_type name (__VA_ARGS__);

DECLARE_ALL_COMMAND_APIS()
#endif


#endif //SIMPLE_CXX_CLIENT_ESP_MODEM_COMMAND_DECLARE_INC
