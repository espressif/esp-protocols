// cat ../include/generate/esp_modem_command_declare.inc | clang -E -P -CC  -xc -I../include -DGENERATE_DOCS  - | sed -n '1,/DCE command documentation/!p' > c_api.h
// cat ../include/generate/esp_modem_command_declare.inc | clang -E -P  -xc -I../include -DGENERATE_DOCS -DGENERATE_RST_LINKS - | sed 's/NL/\n/g' > cxx_api_links.rst

// call parametrs by names for documentation


//  --- DCE command documentation starts here ---
/**
 * @brief Sets the supplied PIN code
 * @param[in] pin Pin
 * @return OK, FAIL or TIMEOUT
 */command_result esp_modem_set_pin (const char* pin); /**
 * @brief Checks if the SIM needs a PIN
 * @param[out] pin_ok true if the SIM card doesn't need a PIN to unlock
 * @return OK, FAIL or TIMEOUT
 */ command_result esp_modem_read_pin (bool* pin_ok); /**
 * @brief Sets echo mode
 * @param[in] echo_on true if echo mode on (repeats the commands)
 * @return OK, FAIL or TIMEOUT
 */ command_result esp_modem_set_echo (const bool echo_on); /**
 * @brief Sets the Txt or Pdu mode for SMS (only txt is supported)
 * @param[in] txt true if txt mode
 * @return OK, FAIL or TIMEOUT
 */ command_result esp_modem_sms_txt_mode (const bool txt); /**
 * @brief Sets the default (GSM) charater set
 * @return OK, FAIL or TIMEOUT
 */ command_result esp_modem_sms_character_set (); /**
 * @brief Sends SMS message in txt mode
 * @param[in] number Phone number to send the message to
 * @param[in] message Text message to be sent
 * @return OK, FAIL or TIMEOUT
 */ command_result esp_modem_send_sms (const char* number, const char* message); /**
 * @brief Resumes data mode (Switches back to th data mode, which was temporarily suspended)
 * @return OK, FAIL or TIMEOUT
 */ command_result esp_modem_resume_data_mode (); /**
 * @brief Sets php context
 * @param[in] x PdP context struct to setup modem cellular connection
 * @return OK, FAIL or TIMEOUT
 */ command_result esp_modem_set_pdp_context (struct PdpContext* x); /**
 * @brief Switches to the command mode
 * @return OK, FAIL or TIMEOUT
 */ command_result esp_modem_set_command_mode (); /**
 * @brief Switches to the CMUX mode
 * @return OK, FAIL or TIMEOUT
 */ command_result esp_modem_set_cmux (); /**
 * @brief Reads the IMSI number
 * @param[out] imsi Module's IMSI number
 * @return OK, FAIL or TIMEOUT
 */ command_result esp_modem_get_imsi (char* imsi); /**
 * @brief Reads the IMEI number
 * @param[out] imei Module's IMEI number
 * @return OK, FAIL or TIMEOUT
 */ command_result esp_modem_get_imei (char* imei); /**
 * @brief Reads the module name
 * @param[out] name module name
 * @return OK, FAIL or TIMEOUT
 */ command_result esp_modem_get_module_name (char* name); /**
 * @brief Sets the modem to data mode
 * @return OK, FAIL or TIMEOUT
 */ command_result esp_modem_set_data_mode (); /**
 * @brief Get Signal quality
 * @param[out] rssi signal strength indication
 * @param[out] ber channel bit error rate
 * @return OK, FAIL or TIMEOUT
 */ command_result esp_modem_get_signal_quality (int* rssi, int* ber);
