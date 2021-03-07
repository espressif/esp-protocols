//
// Created by david on 3/7/21.
//

#ifndef SIMPLE_CXX_CLIENT_ESP_MODEM_COMMAND_DECLARE_INC
#define SIMPLE_CXX_CLIENT_ESP_MODEM_COMMAND_DECLARE_INC

#ifdef __cplusplus
#define STRING_IN const std::string&
#define STRING_OUT std::string&
#else
#define STRING_IN const char*
#define STRING_OUT char*
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
ESP_MODEM_DECLARE_DCE_COMMAND(set_pin,  command_result, TEMPLATE_ARG, MUX_ARG, STRING_IN) \
/**
 * @brief Checks if the SIM needs a PIN
 *
 * @param[out] pin_ok Pin
 */ \
ESP_MODEM_DECLARE_DCE_COMMAND(read_pin, command_result, TEMPLATE_ARG, MUX_ARG, bool& pin_ok)           \
    \
ESP_MODEM_DECLARE_DCE_COMMAND(set_echo, command_result, TEMPLATE_ARG, MUX_ARG, bool on)           \
    \
ESP_MODEM_DECLARE_DCE_COMMAND(resume_data_mode, command_result, TEMPLATE_ARG, MUX_ARG)           \
    \
ESP_MODEM_DECLARE_DCE_COMMAND(set_pdp_context, command_result, TEMPLATE_ARG, MUX_ARG, PdpContext& pdp_context) \
    \
ESP_MODEM_DECLARE_DCE_COMMAND(set_command_mode, command_result, TEMPLATE_ARG, MUX_ARG) \
    \
ESP_MODEM_DECLARE_DCE_COMMAND(set_cmux, command_result, TEMPLATE_ARG, MUX_ARG) \
    \
ESP_MODEM_DECLARE_DCE_COMMAND(get_imsi, command_result, TEMPLATE_ARG, MUX_ARG, std::string& imsi_number) \
    \
ESP_MODEM_DECLARE_DCE_COMMAND(get_imei, command_result, TEMPLATE_ARG, MUX_ARG, std::string& imsi_number) \
    \
/**
 * @brief Reads the module name
 *
 * @param[out]  module name
 */ \
ESP_MODEM_DECLARE_DCE_COMMAND(get_module_name, command_result, TEMPLATE_ARG, MUX_ARG, std::string& name) \
\
/**
 * @brief Sets the modem to data mode
 *
 */ \
ESP_MODEM_DECLARE_DCE_COMMAND(set_data_mode, command_result, TEMPLATE_ARG, MUX_ARG)

//  --- DCE command documentation starts here ---
#ifdef GENERATE_DOCS
// gcc -E -CC -P -DGENERATE_DOCS esp_modem_command_declare.inc | sed -n '/DCE command documentation/,//p'

#define ESP_MODEM_DECLARE_DCE_COMMAND(name, return_type, TEMPLATE_ARG, MUX_ARG, ...) \
return_type name (__VA_ARGS__);

DECLARE_ALL_COMMAND_APIS()
#endif


#endif //SIMPLE_CXX_CLIENT_ESP_MODEM_COMMAND_DECLARE_INC
