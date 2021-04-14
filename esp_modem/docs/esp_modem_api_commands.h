// cat ../include/generate/esp_modem_command_declare.inc | clang -E -P -CC  -xc -I../include -DGENERATE_DOCS  - | sed -n '1,/DCE command documentation/!p' > c_api.h
// cat ../include/generate/esp_modem_command_declare.inc | clang -E -P  -xc -I../include -DGENERATE_DOCS -DGENERATE_RST_LINKS - | sed 's/NL/\n/g' > cxx_api_links.rst
//  --- DCE command documentation starts here ---
/**
 * @brief Sends the supplied PIN code
 *
 * @param pin Pin
 *
 */ command_result esp_modem_set_pin (const char* pin); /**
 * @brief Checks if the SIM needs a PIN
 *
 * @param[out] pin_ok Pin
 */ command_result esp_modem_read_pin (bool* pin_ok); /**
 * @brief Reads the module name
 *
 * @param[out] name module name
 */ command_result esp_modem_set_echo (const bool x); /**
 * @brief Reads the module name
 *
 * @param[out] name module name
 */ command_result esp_modem_resume_data_mode (); /**
 * @brief Reads the module name
 *
 * @param[out] name module name
 */ command_result esp_modem_set_pdp_context (struct PdpContext* x); /**
 * @brief Reads the module name
 *
 * @param[out] name module name
 */ command_result esp_modem_set_command_mode (); /**
 * @brief Reads the module name
 *
 * @param[out] name module name
 */ command_result esp_modem_set_cmux (); /**
 * @brief Reads the module name
 *
 * @param[out] name module name
 */ command_result esp_modem_get_imsi (char* x); /**
 * @brief Reads the module name
 *
 * @param[out] name module name
 */ command_result esp_modem_get_imei (char* x); /**
 * @brief Reads the module name
 *
 * @param[out] name module name
 */ command_result esp_modem_get_module_name (char* name); /**
 * @brief Sets the modem to data mode
 *
 */ command_result esp_modem_set_data_mode (); /**
 * @brief Get Signal quality
 *
 */ command_result esp_modem_get_signal_quality (int* x, int* y);
