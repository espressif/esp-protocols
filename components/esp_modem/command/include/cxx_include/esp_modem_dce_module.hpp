/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>
#include <utility>
//#include "generate/esp_modem_command_declare.inc"
#include "cxx_include/esp_modem_command_library.hpp"
#include "cxx_include/esp_modem_types.hpp"
#include "esp_modem_dce_config.h"


namespace esp_modem {
/**
 * @defgroup ESP_MODEM_MODULE
 * @brief Definition of modules representing specific modem devices
 */
/** @addtogroup ESP_MODEM_MODULE
* @{
*/
enum class command_result;
class DTE;
struct PdpContext;
/**
 * @brief This is a basic building block for custom modules as well as for the supported modules in the esp-modem component
 * It derives from the ModuleIf.
 */
class GenericModule: public ModuleIf {
public:
    /**
     * @brief We can construct a generic device with an existent DTE and it's configuration
     * The configuration could be either the dce-config struct or just a pdp context
     */
    explicit GenericModule(std::shared_ptr<DTE> dte, std::unique_ptr<PdpContext> pdp):
        dte(std::move(dte)), pdp(std::move(pdp)) {}
    explicit GenericModule(std::shared_ptr<DTE> dte, const esp_modem_dce_config *config);
    /**
     * @brief This is a mandatory method for ModuleIf class, which sets up the device
     * to be able to connect to the network. This typically consists of setting basic
     * communication parameters and setting the PDP (defining logical access point
     * to cellular network)
     */
    bool setup_data_mode() override
    {
        if (set_echo(false) != command_result::OK) {
            return false;
        }
        if (set_pdp_context(*pdp) != command_result::OK) {
            return false;
        }
        return true;
    }
    /**
     * @brief This is a mandatory method of ModuleIf class, which defines
     * basic commands for switching between DATA, COMMAND and CMUX modes
     */
    bool set_mode(modem_mode mode) override
    {
        if (mode == modem_mode::DATA_MODE) {
            if (set_data_mode() != command_result::OK) {
                return resume_data_mode() == command_result::OK;
            }
            return true;
        } else if (mode == modem_mode::COMMAND_MODE) {
            Task::Delay(1000); // Mandatory 1s pause before
            int retry = 0;
            while (retry++ < 3) {
                if (set_command_mode() == command_result::OK) {
                    return true;
                }
                // send a newline to delimit the escape from the upcoming sync command
                uint8_t delim = '\n';
                dte->write(&delim, 1);
                if (sync() == command_result::OK) {
                    return true;
                }
                Task::Delay(1000); // Mandatory 1s pause before escape
            }
            return false;
        } else if (mode == modem_mode::CMUX_MODE) {
            return set_cmux() == command_result::OK;
        }
        return true;
    }
    /**
     * @brief Additional method providing runtime configuration of PDP context
     */
    void configure_pdp_context(std::unique_ptr<PdpContext> new_pdp)
    {
        pdp = std::move(new_pdp);
    }
    /**
     * @brief Simplified version of operator name (without the ACT, which is included in the command library)
     */
    command_result get_operator_name(std::string &name)
    {
        int dummy_act;
        return get_operator_name(name, dummy_act);
    }
    /**
     * @brief Common DCE commands generated from the API AT list
     */
//    DECLARE_ALL_COMMAND_APIS(virtual return_type name(...); )
    /**
     * @brief Sends the initial AT sequence to sync up with the device
     * @return OK, FAIL or TIMEOUT
     */
    virtual command_result sync();
    /**
     * @brief Reads the operator name
     * @param[out] name operator name
     * @param[out] act access technology
     * @return OK, FAIL or TIMEOUT
     */
    virtual command_result get_operator_name(std::string &name, int &act);
    /**
     * @brief Stores current user profile
     * @return OK, FAIL or TIMEOUT
     */
    virtual command_result store_profile();
    /**
     * @brief Sets the supplied PIN code
     * @param[in] pin Pin
     * @return OK, FAIL or TIMEOUT
     */
    virtual command_result set_pin(const std::string &pin);
    /**
     * @brief Execute the supplied AT command in raw mode (doesn't append '\r' to command, returns everything)
     * @param[in] cmd String command that's send to DTE
     * @param[out] out Raw output from DTE
     * @param[in] pass Pattern in response for the API to return OK
     * @param[in] fail Pattern in response for the API to return FAIL
     * @param[in] timeout AT command timeout in milliseconds
     * @return OK, FAIL or TIMEOUT
     */
    virtual command_result at_raw(const std::string &cmd, std::string &out, const std::string &pass, const std::string &fail, int timeout);
    /**
     * @brief Execute the supplied AT command
     * @param[in] cmd AT command
     * @param[out] out Command output string
     * @param[in] timeout AT command timeout in milliseconds
     * @return OK, FAIL or TIMEOUT
     */
    virtual command_result at(const std::string &cmd, std::string &out, int timeout);
    /**
     * @brief Checks if the SIM needs a PIN
     * @param[out] pin_ok true if the SIM card doesn't need a PIN to unlock
     * @return OK, FAIL or TIMEOUT
     */
    virtual command_result read_pin(bool &pin_ok);
    /**
     * @brief Sets echo mode
     * @param[in] echo_on true if echo mode on (repeats the commands)
     * @return OK, FAIL or TIMEOUT
     */
    virtual command_result set_echo(const bool echo_on);
    /**
     * @brief Sets the Txt or Pdu mode for SMS (only txt is supported)
     * @param[in] txt true if txt mode
     * @return OK, FAIL or TIMEOUT
     */
    virtual command_result sms_txt_mode(const bool txt);
    /**
     * @brief Sets the default (GSM) character set
     * @return OK, FAIL or TIMEOUT
     */
    virtual command_result sms_character_set();
    /**
     * @brief Sends SMS message in txt mode
     * @param[in] number Phone number to send the message to
     * @param[in] message Text message to be sent
     * @return OK, FAIL or TIMEOUT
     */
    virtual command_result send_sms(const std::string &number, const std::string &message);
    /**
     * @brief Resumes data mode (Switches back to the data mode, which was temporarily suspended)
     * @return OK, FAIL or TIMEOUT
     */
    virtual command_result resume_data_mode();
    /**
     * @brief Sets php context
     * @param[in] p1 PdP context struct to setup modem cellular connection
     * @return OK, FAIL or TIMEOUT
     */
    virtual command_result set_pdp_context(PdpContext &pdp);
    /**
     * @brief Switches to the command mode
     * @return OK, FAIL or TIMEOUT
     */
    virtual command_result set_command_mode();
    /**
     * @brief Switches to the CMUX mode
     * @return OK, FAIL or TIMEOUT
     */
    virtual command_result set_cmux();
    /**
     * @brief Reads the IMSI number
     * @param[out] imsi Module's IMSI number
     * @return OK, FAIL or TIMEOUT
     */
    virtual command_result get_imsi(std::string &imsi);
    /**
     * @brief Reads the IMEI number
     * @param[out] imei Module's IMEI number
     * @return OK, FAIL or TIMEOUT
     */
    virtual command_result get_imei(std::string &imei);
    /**
     * @brief Reads the module name
     * @param[out] name module name
     * @return OK, FAIL or TIMEOUT
     */
    virtual command_result get_module_name(std::string &name);
    /**
     * @brief Sets the modem to data mode
     * @return OK, FAIL or TIMEOUT
     */
    virtual command_result set_data_mode();
    /**
     * @brief Get Signal quality
     * @param[out] rssi signal strength indication
     * @param[out] ber channel bit error rate
     * @return OK, FAIL or TIMEOUT
     */
    virtual command_result get_signal_quality(int &rssi, int &ber);
    /**
     * @brief Sets HW control flow
     * @param[in] dce_flow 0=none, 2=RTS hw flow control of DCE
     * @param[in] dte_flow 0=none, 2=CTS hw flow control of DTE
     * @return OK, FAIL or TIMEOUT
     */
    virtual command_result set_flow_control(int dce_flow, int dte_flow);
    /**
     * @brief Hangs up current data call
     * @return OK, FAIL or TIMEOUT
     */
    virtual command_result hang_up();
    /**
     * @brief Get voltage levels of modem power up circuitry
     * @param[out] voltage Current status in mV
     * @param[out] bcs charge status (-1-Not available, 0-Not charging, 1-Charging, 2-Charging done)
     * @param[out] bcl 1-100% battery capacity, -1-Not available
     * @return OK, FAIL or TIMEOUT
     */
    virtual command_result get_battery_status(int &voltage, int &bcs, int &bcl);
    /**
     * @brief Power down the module
     * @return OK, FAIL or TIMEOUT
     */
    virtual command_result power_down();
    /**
     * @brief Reset the module
     * @return OK, FAIL or TIMEOUT
     */
    virtual command_result reset();
    /**
     * @brief Configures the baudrate
     * @param[in] baud Desired baud rate of the DTE
     * @return OK, FAIL or TIMEOUT
     */
    virtual command_result set_baud(int baud);
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
    virtual command_result set_operator(int mode, int format, const std::string &oper);
    /**
     * @brief Attach or detach from the GPRS service
     * @param[in] state 1-attach 0-detach
     * @return OK, FAIL or TIMEOUT
     */
    virtual command_result set_network_attachment_state(int state);
    /**
     * @brief Get network attachment state
     * @param[out] state 1-attached 0-detached
     * @return OK, FAIL or TIMEOUT
     */
    virtual command_result get_network_attachment_state(int &state);
    /**
     * @brief What mode the radio should be set to
     * @param[in] state state 1-full 0-minimum ...
     * @return OK, FAIL or TIMEOUT
     */
    virtual command_result set_radio_state(int state);
    /**
     * @brief Get current radio state
     * @param[out] state 1-full 0-minimum ...
     * @return OK, FAIL or TIMEOUT
     */
    virtual command_result get_radio_state(int &state);
    /**
     * @brief Set network mode
     * @param[in] mode preferred mode
     * @return OK, FAIL or TIMEOUT
     */
    virtual command_result set_network_mode(int mode);
    /**
     * @brief Preferred network mode (CAT-M and/or NB-IoT)
     * @param[in] mode preferred selection
     * @return OK, FAIL or TIMEOUT
     */
    virtual command_result set_preferred_mode(int mode);
    /**
     * @brief Set network bands for CAT-M or NB-IoT
     * @param[in] mode CAT-M or NB-IoT
     * @param[in] bands bitmap in hex representing bands
     * @param[in] size size of teh bands bitmap
     * @return OK, FAIL or TIMEOUT
     */
    virtual command_result set_network_bands(const std::string &mode, const int *bands, int size);
    /**
     * @brief Show network system mode
     * @param[out] mode current network mode
     * @return OK, FAIL or TIMEOUT
     */
    virtual command_result get_network_system_mode(int &mode);
    /**
     * @brief GNSS power control
     * @param[out] mode power mode (0 - off, 1 - on)
     * @return OK, FAIL or TIMEOUT
     */
    virtual command_result set_gnss_power_mode(int mode);
    /**
     * @brief GNSS power control
     * @param[out] mode power mode (0 - off, 1 - on)
     * @return OK, FAIL or TIMEOUT
     */
    virtual command_result get_gnss_power_mode(int &mode);
    /**
     * @brief Configure PSM
     * @param[in] mode psm mode (0 - off, 1 - on, 2 - off & discard stored params)
     * @return OK, FAIL or TIMEOUT
     */
    virtual command_result config_psm(int mode, const std::string &tau, const std::string &active_time);
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
    virtual command_result config_network_registration_urc(int value);
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
    virtual command_result get_network_registration_state(int &state);
    /**
     *  @brief Configures the mobile termination error (+CME ERROR)
     *  @param[in] mode The form of the final result code
     *  mode = 0 - Disable, use and send ERROR instead
     *  mode = 1 - Enable, use numeric error values
     *  mode = 2 - Enable, result code and use verbose error values
     */
    virtual command_result config_mobile_termination_error(int mode);
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
    virtual command_result config_edrx(int mode, int access_technology, const std::string &edrx_value);
protected:
    std::shared_ptr<DTE> dte; /*!< Generic device needs the DTE as a channel talk to the module using AT commands */
    std::unique_ptr<PdpContext> pdp; /*!< It also needs a PDP data, const information used for setting up cellular network */
};
// Definitions of other supported modules with some specific commands overwritten
/**
 * @brief Specific definition of the SIM7600 module
 */
class SIM7600: public GenericModule {
    using GenericModule::GenericModule;
public:
    command_result get_battery_status(int &voltage, int &bcs, int &bcl) override;
    command_result power_down() override;
    command_result set_gnss_power_mode(int mode) override;
    command_result set_network_bands(const std::string &mode, const int *bands, int size) override;
};
/**
 * @brief Specific definition of the SIM7070 module
 */
class SIM7070: public GenericModule {
    using GenericModule::GenericModule;
public:
    command_result power_down() override;
    command_result set_data_mode() override;
};
/**
 * @brief Specific definition of the SIM7000 module
 */
class SIM7000: public GenericModule {
    using GenericModule::GenericModule;
public:
    command_result power_down() override;
};
/**
 * @brief Specific definition of the SIM800 module
 */
class SIM800: public GenericModule {
    using GenericModule::GenericModule;
public:
    command_result power_down() override;
};
/**
 * @brief Specific definition of the BG96 module
 */
class BG96: public GenericModule {
    using GenericModule::GenericModule;
public:
    command_result set_pdp_context(PdpContext &pdp) override;
};
class SQNGM02S : public GenericModule {
    using GenericModule::GenericModule;
public:
    command_result connect(PdpContext &pdp);
    bool setup_data_mode() override;
};
/**
 * @}
 */
} // namespace esp_modem
