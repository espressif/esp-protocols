/**
 * @brief Sends the initial AT sequence to sync up with the device
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(sync, command_result)

/**
 * @brief Reads the operator name
 * @param[out] name operator name
 * @param[out] act access technology
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(get_operator_name, command_result, STR_OUT(name), INT_OUT(act))

/**
 * @brief Stores current user profile
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(store_profile, command_result)

/**
 * @brief Sets the supplied PIN code
 * @param[in] pin Pin
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(set_pin, command_result, STR_IN(pin))

/**
 * @brief Execute the supplied AT command in raw mode (doesn't append '\r' to command, returns everything)
 * @param[in] cmd String command that's send to DTE
 * @param[out] out Raw output from DTE
 * @param[in] pass Pattern in response for the API to return OK
 * @param[in] fail Pattern in response for the API to return FAIL
 * @param[in] timeout AT command timeout in milliseconds
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(at_raw, command_result, STR_IN(cmd), STR_OUT(out), STR_IN(pass), STR_IN(fail), INT_IN(timeout))

/**
 * @brief Execute the supplied AT command
 * @param[in] cmd AT command
 * @param[out] out Command output string
 * @param[in] timeout AT command timeout in milliseconds
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(at, command_result, STR_IN(cmd), STR_OUT(out), INT_IN(timeout))

/**
 * @brief Checks if the SIM needs a PIN
 * @param[out] pin_ok true if the SIM card doesn't need a PIN to unlock
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(read_pin, command_result, BOOL_OUT(pin_ok))

/**
 * @brief Sets echo mode
 * @param[in] echo_on true if echo mode on (repeats the commands)
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(set_echo, command_result, BOOL_IN(echo_on))

/**
 * @brief Sets the Txt or Pdu mode for SMS (only txt is supported)
 * @param[in] txt true if txt mode
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(sms_txt_mode, command_result, BOOL_IN(txt))

/**
 * @brief Sets the default (GSM) character set
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(sms_character_set, command_result)

/**
 * @brief Sends SMS message in txt mode
 * @param[in] number Phone number to send the message to
 * @param[in] message Text message to be sent
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(send_sms, command_result, STR_IN(number), STR_IN(message))

/**
 * @brief Resumes data mode (Switches back to the data mode, which was temporarily suspended)
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(resume_data_mode, command_result)

/**
 * @brief Sets php context
 * @param[in] p1 PdP context struct to setup modem cellular connection
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(set_pdp_context, command_result, STRUCT_OUT(PdpContext, pdp))

/**
 * @brief Switches to the command mode
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(set_command_mode, command_result)

/**
 * @brief Switches to the CMUX mode
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(set_cmux, command_result)

/**
 * @brief Reads the IMSI number
 * @param[out] imsi Module's IMSI number
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(get_imsi, command_result, STR_OUT(imsi))

/**
 * @brief Reads the IMEI number
 * @param[out] imei Module's IMEI number
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(get_imei, command_result, STR_OUT(imei))

/**
 * @brief Reads the module name
 * @param[out] name module name
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(get_module_name, command_result, STR_OUT(name))

/**
 * @brief Sets the modem to data mode
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(set_data_mode, command_result)

/**
 * @brief Get Signal quality
 * @param[out] rssi signal strength indication
 * @param[out] ber channel bit error rate
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(get_signal_quality, command_result, INT_OUT(rssi), INT_OUT(ber))

/**
 * @brief Sets HW control flow
 * @param[in] dce_flow 0=none, 2=RTS hw flow control of DCE
 * @param[in] dte_flow 0=none, 2=CTS hw flow control of DTE
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(set_flow_control, command_result, INT_IN(dce_flow), INT_IN(dte_flow))

/**
 * @brief Hangs up current data call
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(hang_up, command_result)

/**
 * @brief Get voltage levels of modem power up circuitry
 * @param[out] voltage Current status in mV
 * @param[out] bcs charge status (-1-Not available, 0-Not charging, 1-Charging, 2-Charging done)
 * @param[out] bcl 1-100% battery capacity, -1-Not available
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(get_battery_status, command_result, INT_OUT(voltage), INT_OUT(bcs), INT_OUT(bcl))

/**
 * @brief Power down the module
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(power_down, command_result)

/**
 * @brief Reset the module
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(reset, command_result)

/**
 * @brief Configures the baudrate
 * @param[in] baud Desired baud rate of the DTE
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(set_baud, command_result, INT_IN(baud))

/**
 * @brief Force an attempt to connect to a specific operator
 * @param[in] mode mode of attempt
 * mode=0 - automatic
 * mode=1 - manual
 * mode=2 - deregister
 * mode=3 - set format for read operation
 * mode=4 - manual with fallback to automatic
 * @param[in] format what format the operator is given in
 * format=0 - long format
 * format=1 - short format
 * format=2 - numeric
 * @param[in] oper the operator to connect to
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(set_operator, command_result, INT_IN(mode), INT_IN(format), STR_IN(oper))

/**
 * @brief Attach or detach from the GPRS service
 * @param[in] state 1-attach 0-detach
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(set_network_attachment_state, command_result, INT_IN(state))

/**
 * @brief Get network attachment state
 * @param[out] state 1-attached 0-detached
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(get_network_attachment_state, command_result, INT_OUT(state))

/**
 * @brief What mode the radio should be set to
 * @param[in] state state 1-full 0-minimum ...
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(set_radio_state, command_result, INT_IN(state))

/**
 * @brief Get current radio state
 * @param[out] state 1-full 0-minimum ...
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(get_radio_state, command_result, INT_OUT(state))

/**
 * @brief Set network mode
 * @param[in] mode preferred mode
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(set_network_mode, command_result, INT_IN(mode))

/**
 * @brief Preferred network mode (CAT-M and/or NB-IoT)
 * @param[in] mode preferred selection
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(set_preferred_mode, command_result, INT_IN(mode))

/**
 * @brief Set network bands for CAT-M or NB-IoT
 * @param[in] mode CAT-M or NB-IoT
 * @param[in] bands bitmap in hex representing bands
 * @param[in] size size of teh bands bitmap
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(set_network_bands, command_result, STR_IN(mode), INT_LIST_IN(bands), INT_IN(size))

/**
 * @brief Show network system mode
 * @param[out] mode current network mode
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(get_network_system_mode, command_result, INT_OUT(mode))

/**
 * @brief GNSS power control
 * @param[out] mode power mode (0 - off, 1 - on)
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(set_gnss_power_mode, command_result, INT_IN(mode))

/**
 * @brief GNSS power control
 * @param[out] mode power mode (0 - off, 1 - on)
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(get_gnss_power_mode, command_result, INT_OUT(mode))

/**
 * @brief Configure PSM
 * @param[in] mode psm mode (0 - off, 1 - on, 2 - off & discard stored params)
 * @return OK, FAIL or TIMEOUT
 */
ESP_MODEM_DECLARE_DCE_COMMAND(config_psm, command_result, INT_IN(mode), STR_IN(tau), STR_IN(active_time))

/**
 * @brief Configure CEREG urc
 * @param[in] value
 * value = 0 - Disable network URC
 * value = 1 - Enable network URC
 * value = 2 - Enable network URC with location information
 * value = 3 - Enable network URC with location information and EMM cause
 * value = 4 - Enable network URC with location information and PSM value
 * value = 5 - Enable network URC with location information and PSM value, EMM cause
 */
ESP_MODEM_DECLARE_DCE_COMMAND(config_network_registration_urc, command_result, INT_IN(value))

/**
 *  @brief Gets the current network registration state
 *  @param[out] state The current network registration state
 *  state = 0 - Not registered, MT is not currently searching an operator to register to
 *  state = 1 - Registered, home network
 *  state = 2 - Not registered, but MT is currently trying to attach or searching an operator to register to
 *  state = 3 - Registration denied
 *  state = 4 - Unknown
 *  state = 5 - Registered, Roaming
 *  state = 6 - Registered, for SMS only, home network (NB-IoT only)
 *  state = 7 - Registered, for SMS only, roaming (NB-IoT only)
 *  state = 8 - Attached for emergency bearer services only
 *  state = 9 - Registered for CSFB not preferred, home network
 *  state = 10 - Registered for CSFB not preferred, roaming
 */
ESP_MODEM_DECLARE_DCE_COMMAND(get_network_registration_state, command_result, INT_OUT(state))

/**
 *  @brief Configures the mobile termination error (+CME ERROR)
 *  @param[in] mode The form of the final result code
 *  mode = 0 - Disable, use and send ERROR instead
 *  mode = 1 - Enable, use numeric error values
 *  mode = 2 - Enable, result code and use verbose error values
 */
ESP_MODEM_DECLARE_DCE_COMMAND(config_mobile_termination_error, command_result, INT_IN(mode))

/**
 * @brief Configure eDRX
 * @param[in] mode
 * mode = 0 - Disable
 * mode = 1 - Enable
 * mode = 2 - Enable + URC
 * mode = 3 - Disable + Reset parameter.
 * @param[in] access_technology
 * act = 0 - ACT is not using eDRX (used in URC)
 * act = 1 - EC-GSM-IoT (A/Gb mode)
 * act = 2 - GSM (A/Gb mode)
 * act = 3 - UTRAN (Iu mode)
 * act = 4 - E-UTRAN (WB-S1 mode)
 * act = 5 - E-UTRAN (NB-S1 mode)
 * @param[in] edrx_value nible string containing encoded eDRX time
 * @param[in] ptw_value nible string containing encoded Paging Time Window
 */
ESP_MODEM_DECLARE_DCE_COMMAND(config_edrx, command_result, INT_IN(mode), INT_IN(access_technology), STR_IN(edrx_value))

#ifdef GENERATE_DOCS
// cat ../include/generate/esp_modem_command_declare.inc | clang++ -E -P -CC  -xc++ -I../include -DGENERATE_DOCS  - | sed -n '1,/DCE command documentation/!p'
// cat ../include/generate/esp_modem_command_declare.inc | clang -E -P -CC  -xc -I../include -DGENERATE_DOCS  - | sed -n '1,/DCE command documentation/!p' > c_api.h
// cat ../include/generate/esp_modem_command_declare.inc | clang -E -P  -xc -I../include -DGENERATE_DOCS -DGENERATE_RST_LINKS - | sed 's/NL/\n/g' > cxx_api_links.rst

// call parametrs by names for documentation
#undef _ARG
#define _ARG(param, name) name
//  --- DCE command documentation starts here ---
#ifdef __cplusplus
class esp_modem::DCE : public DCE_T<GenericModule> {
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
