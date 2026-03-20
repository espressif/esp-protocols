/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "cxx_include/esp_modem_dte.hpp"
#include "cxx_include/esp_modem_dce_module.hpp"
#include "cxx_include/esp_modem_types.hpp"

namespace esp_modem {
namespace dce_commands {
/**
 * @defgroup ESP_MODEM_DCE_COMMAND ESP_MODEM DCE command library
 * @brief Library of the most useful DCE commands
 */
/** @addtogroup ESP_MODEM_DCE_COMMAND
 * @{
 */
/**
 * @brief Generic AT command to be send with pass and fail phrases
 *
 * @param t Commandable object (anything that can accept commands)
 * @param command Command to be sent do the commandable object
 * @param pass_phrase String to be present in the reply to pass this command
 * @param fail_phrase String to be present in the reply to fail this command
 * @param timeout_ms Timeout in ms
 */
command_result generic_command(CommandableIf *t, const std::string &command,
                               const std::string &pass_phrase,
                               const std::string &fail_phrase, uint32_t timeout_ms);
/**
 * @brief Declaration of all commands is generated from esp_modem_command_declare.inc
 */
/**
 * @brief Sends the initial AT sequence to sync up with the device
 * @return OK, FAIL or TIMEOUT
 */
command_result sync(CommandableIf *t);
/**
 * @brief Reads the operator name
 * @param[out] name operator name
 * @param[out] act access technology
 * @return OK, FAIL or TIMEOUT
 */
command_result get_operator_name(CommandableIf *t, std::string &name, int &act);
/**
 * @brief Stores current user profile
 * @return OK, FAIL or TIMEOUT
 */
command_result store_profile(CommandableIf *t);
/**
 * @brief Sets the supplied PIN code
 * @param[in] pin Pin
 * @return OK, FAIL or TIMEOUT
 */
command_result set_pin(CommandableIf *t, const std::string &pin);
/**
 * @brief Execute the supplied AT command in raw mode (doesn't append '\r' to command, returns everything)
 * @param[in] cmd String command that's send to DTE
 * @param[out] out Raw output from DTE
 * @param[in] pass Pattern in response for the API to return OK
 * @param[in] fail Pattern in response for the API to return FAIL
 * @param[in] timeout AT command timeout in milliseconds
 * @return OK, FAIL or TIMEOUT
 */
command_result at_raw(CommandableIf *t, const std::string &cmd, std::string &out, const std::string &pass, const std::string &fail, int timeout);
/**
 * @brief Execute the supplied AT command
 * @param[in] cmd AT command
 * @param[out] out Command output string
 * @param[in] timeout AT command timeout in milliseconds
 * @return OK, FAIL or TIMEOUT
 */
command_result at(CommandableIf *t, const std::string &cmd, std::string &out, int timeout);
/**
 * @brief Checks if the SIM needs a PIN
 * @param[out] pin_ok true if the SIM card doesn't need a PIN to unlock
 * @return OK, FAIL or TIMEOUT
 */
command_result read_pin(CommandableIf *t, bool &pin_ok);
/**
 * @brief Sets echo mode
 * @param[in] echo_on true if echo mode on (repeats the commands)
 * @return OK, FAIL or TIMEOUT
 */
command_result set_echo(CommandableIf *t, const bool echo_on);
/**
 * @brief Sets the Txt or Pdu mode for SMS (only txt is supported)
 * @param[in] txt true if txt mode
 * @return OK, FAIL or TIMEOUT
 */
command_result sms_txt_mode(CommandableIf *t, const bool txt);
/**
 * @brief Sets the default (GSM) character set
 * @return OK, FAIL or TIMEOUT
 */
command_result sms_character_set(CommandableIf *t);
/**
 * @brief Sends SMS message in txt mode
 * @param[in] number Phone number to send the message to
 * @param[in] message Text message to be sent
 * @return OK, FAIL or TIMEOUT
 */
command_result send_sms(CommandableIf *t, const std::string &number, const std::string &message);
/**
 * @brief Resumes data mode (Switches back to the data mode, which was temporarily suspended)
 * @return OK, FAIL or TIMEOUT
 */
command_result resume_data_mode(CommandableIf *t);
/**
 * @brief Sets php context
 * @param[in] p1 PdP context struct to setup modem cellular connection
 * @return OK, FAIL or TIMEOUT
 */
command_result set_pdp_context(CommandableIf *t, PdpContext &pdp);
/**
 * @brief Switches to the command mode
 * @return OK, FAIL or TIMEOUT
 */
command_result set_command_mode(CommandableIf *t);
/**
 * @brief Switches to the CMUX mode
 * @return OK, FAIL or TIMEOUT
 */
command_result set_cmux(CommandableIf *t);
/**
 * @brief Reads the IMSI number
 * @param[out] imsi Module's IMSI number
 * @return OK, FAIL or TIMEOUT
 */
command_result get_imsi(CommandableIf *t, std::string &imsi);
/**
 * @brief Reads the IMEI number
 * @param[out] imei Module's IMEI number
 * @return OK, FAIL or TIMEOUT
 */
command_result get_imei(CommandableIf *t, std::string &imei);
/**
 * @brief Reads the module name
 * @param[out] name module name
 * @return OK, FAIL or TIMEOUT
 */
command_result get_module_name(CommandableIf *t, std::string &name);
/**
 * @brief Sets the modem to data mode
 * @return OK, FAIL or TIMEOUT
 */
command_result set_data_mode(CommandableIf *t);
/**
 * @brief Get Signal quality
 * @param[out] rssi signal strength indication
 * @param[out] ber channel bit error rate
 * @return OK, FAIL or TIMEOUT
 */
command_result get_signal_quality(CommandableIf *t, int &rssi, int &ber);
/**
 * @brief Sets HW control flow
 * @param[in] dce_flow 0=none, 2=RTS hw flow control of DCE
 * @param[in] dte_flow 0=none, 2=CTS hw flow control of DTE
 * @return OK, FAIL or TIMEOUT
 */
command_result set_flow_control(CommandableIf *t, int dce_flow, int dte_flow);
/**
 * @brief Hangs up current data call
 * @return OK, FAIL or TIMEOUT
 */
command_result hang_up(CommandableIf *t);
/**
 * @brief Get voltage levels of modem power up circuitry
 * @param[out] voltage Current status in mV
 * @param[out] bcs charge status (-1-Not available, 0-Not charging, 1-Charging, 2-Charging done)
 * @param[out] bcl 1-100% battery capacity, -1-Not available
 * @return OK, FAIL or TIMEOUT
 */
command_result get_battery_status(CommandableIf *t, int &voltage, int &bcs, int &bcl);
/**
 * @brief Power down the module
 * @return OK, FAIL or TIMEOUT
 */
command_result power_down(CommandableIf *t);
/**
 * @brief Reset the module
 * @return OK, FAIL or TIMEOUT
 */
command_result reset(CommandableIf *t);
/**
 * @brief Configures the baudrate
 * @param[in] baud Desired baud rate of the DTE
 * @return OK, FAIL or TIMEOUT
 */
command_result set_baud(CommandableIf *t, int baud);
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
command_result set_operator(CommandableIf *t, int mode, int format, const std::string &oper);
/**
 * @brief Attach or detach from the GPRS service
 * @param[in] state 1-attach 0-detach
 * @return OK, FAIL or TIMEOUT
 */
command_result set_network_attachment_state(CommandableIf *t, int state);
/**
 * @brief Get network attachment state
 * @param[out] state 1-attached 0-detached
 * @return OK, FAIL or TIMEOUT
 */
command_result get_network_attachment_state(CommandableIf *t, int &state);
/**
 * @brief What mode the radio should be set to
 * @param[in] state state 1-full 0-minimum ...
 * @return OK, FAIL or TIMEOUT
 */
command_result set_radio_state(CommandableIf *t, int state);
/**
 * @brief Get current radio state
 * @param[out] state 1-full 0-minimum ...
 * @return OK, FAIL or TIMEOUT
 */
command_result get_radio_state(CommandableIf *t, int &state);
/**
 * @brief Set network mode
 * @param[in] mode preferred mode
 * @return OK, FAIL or TIMEOUT
 */
command_result set_network_mode(CommandableIf *t, int mode);
/**
 * @brief Preferred network mode (CAT-M and/or NB-IoT)
 * @param[in] mode preferred selection
 * @return OK, FAIL or TIMEOUT
 */
command_result set_preferred_mode(CommandableIf *t, int mode);
/**
 * @brief Set network bands for CAT-M or NB-IoT
 * @param[in] mode CAT-M or NB-IoT
 * @param[in] bands bitmap in hex representing bands
 * @param[in] size size of teh bands bitmap
 * @return OK, FAIL or TIMEOUT
 */
command_result set_network_bands(CommandableIf *t, const std::string &mode, const int *bands, int size);
/**
 * @brief Show network system mode
 * @param[out] mode current network mode
 * @return OK, FAIL or TIMEOUT
 */
command_result get_network_system_mode(CommandableIf *t, int &mode);
/**
 * @brief GNSS power control
 * @param[out] mode power mode (0 - off, 1 - on)
 * @return OK, FAIL or TIMEOUT
 */
command_result set_gnss_power_mode(CommandableIf *t, int mode);
/**
 * @brief GNSS power control
 * @param[out] mode power mode (0 - off, 1 - on)
 * @return OK, FAIL or TIMEOUT
 */
command_result get_gnss_power_mode(CommandableIf *t, int &mode);
/**
 * @brief Configure PSM
 * @param[in] mode psm mode (0 - off, 1 - on, 2 - off & discard stored params)
 * @return OK, FAIL or TIMEOUT
 */
command_result config_psm(CommandableIf *t, int mode, const std::string &tau, const std::string &active_time);
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
command_result config_network_registration_urc(CommandableIf *t, int value);
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
command_result get_network_registration_state(CommandableIf *t, int &state);
/**
 *  @brief Configures the mobile termination error (+CME ERROR)
 *  @param[in] mode The form of the final result code
 *  mode = 0 - Disable, use and send ERROR instead
 *  mode = 1 - Enable, use numeric error values
 *  mode = 2 - Enable, result code and use verbose error values
 */
command_result config_mobile_termination_error(CommandableIf *t, int mode);
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
command_result config_edrx(CommandableIf *t, int mode, int access_technology, const std::string &edrx_value);
/**
 * @brief Following commands that are different for some specific modules
 */
command_result get_battery_status_sim7xxx(CommandableIf *t, int &voltage, int &bcs, int &bcl);
command_result set_gnss_power_mode_sim76xx(CommandableIf *t, int mode);
command_result power_down_sim76xx(CommandableIf *t);
command_result power_down_sim70xx(CommandableIf *t);
command_result set_network_bands_sim76xx(CommandableIf *t, const std::string &mode, const int *bands, int size);
command_result power_down_sim8xx(CommandableIf *t);
command_result set_data_mode_alt(CommandableIf *t);
command_result set_pdp_context(CommandableIf *t, PdpContext &pdp, uint32_t timeout_ms);
/**
 * @}
 */
} // dce_commands
} // esp_modem
