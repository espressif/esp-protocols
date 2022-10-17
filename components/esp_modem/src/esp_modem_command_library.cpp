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

#include <charconv>
#include <list>
#include "esp_log.h"
#include "cxx_include/esp_modem_dte.hpp"
#include "cxx_include/esp_modem_dce_module.hpp"
#include "cxx_include/esp_modem_command_library.hpp"

namespace esp_modem::dce_commands {

static const char *TAG = "command_lib";

command_result generic_command(CommandableIf *t, const std::string &command,
                               const std::list<std::string_view> &pass_phrase,
                               const std::list<std::string_view> &fail_phrase,
                               uint32_t timeout_ms)
{
    ESP_LOGD(TAG, "%s command %s\n", __func__, command.c_str());
    return t->command(command, [&](uint8_t *data, size_t len) {
        std::string_view response((char *)data, len);
        if (data == nullptr || len == 0 || response.empty()) {
            return command_result::TIMEOUT;
        }
        ESP_LOGD(TAG, "Response: %.*s\n", (int)response.length(), response.data());
        for (auto &it : pass_phrase)
            if (response.find(it) != std::string::npos) {
                return command_result::OK;
            }
        for (auto &it : fail_phrase)
            if (response.find(it) != std::string::npos) {
                return command_result::FAIL;
            }
        return command_result::TIMEOUT;
    }, timeout_ms);

}

static inline command_result generic_command(CommandableIf *t, const std::string &command,
        const std::string &pass_phrase,
        const std::string &fail_phrase, uint32_t timeout_ms)
{
    ESP_LOGV(TAG, "%s", __func__ );
    const auto pass = std::list<std::string_view>({pass_phrase});
    const auto fail = std::list<std::string_view>({fail_phrase});
    return generic_command(t, command, pass, fail, timeout_ms);
}

static inline command_result generic_get_string(CommandableIf *t, const std::string &command, std::string_view &output, uint32_t timeout_ms = 500)
{
    ESP_LOGV(TAG, "%s", __func__ );
    return t->command(command, [&](uint8_t *data, size_t len) {
        size_t pos = 0;
        std::string_view response((char *)data, len);
        while ((pos = response.find('\n')) != std::string::npos) {
            std::string_view token = response.substr(0, pos);
            for (auto it = token.end() - 1; it > token.begin(); it--) // strip trailing CR or LF
                if (*it == '\r' || *it == '\n') {
                    token.remove_suffix(1);
                }
            ESP_LOGV(TAG, "Token: {%.*s}\n", static_cast<int>(token.size()), token.data());

            if (token.find("OK") != std::string::npos) {
                return command_result::OK;
            } else if (token.find("ERROR") != std::string::npos) {
                return command_result::FAIL;
            } else if (token.size() > 2) {
                output = token;
            }
            response = response.substr(pos + 1);
        }
        return command_result::TIMEOUT;
    }, timeout_ms);
}

static inline command_result generic_get_string(CommandableIf *t, const std::string &command, std::string &output, uint32_t timeout_ms = 500)
{
    ESP_LOGV(TAG, "%s", __func__ );
    std::string_view out;
    auto ret = generic_get_string(t, command, out, timeout_ms);
    if (ret == command_result::OK) {
        output = out;
    }
    return ret;
}


static inline command_result generic_command_common(CommandableIf *t, const std::string &command, uint32_t timeout = 500)
{
    ESP_LOGV(TAG, "%s", __func__ );
    return generic_command(t, command, "OK", "ERROR", timeout);
}

command_result sync(CommandableIf *t)
{
    ESP_LOGV(TAG, "%s", __func__ );
    return generic_command_common(t, "AT\r");
}

command_result store_profile(CommandableIf *t)
{
    ESP_LOGV(TAG, "%s", __func__ );
    return generic_command_common(t, "AT&W\r");
}

command_result power_down(CommandableIf *t)
{
    ESP_LOGV(TAG, "%s", __func__ );
    return generic_command(t, "AT+QPOWD=1\r", "POWERED DOWN", "ERROR", 1000);
}

command_result power_down_sim7xxx(CommandableIf *t)
{
    ESP_LOGV(TAG, "%s", __func__ );
    return generic_command_common(t, "AT+CPOF\r", 1000);
}

command_result power_down_sim8xx(CommandableIf *t)
{
    ESP_LOGV(TAG, "%s", __func__ );
    return generic_command(t, "AT+CPOWD=1\r", "POWER DOWN", "ERROR", 1000);
}

command_result reset(CommandableIf *t)
{
    ESP_LOGV(TAG, "%s", __func__ );
    return generic_command(t,  "AT+CRESET\r", "PB DONE", "ERROR", 60000);
}

command_result set_baud(CommandableIf *t, int baud)
{
    ESP_LOGV(TAG, "%s", __func__ );
    return generic_command_common(t,  "AT+IPR=" + std::to_string(baud) + "\r");
}

command_result hang_up(CommandableIf *t)
{
    ESP_LOGV(TAG, "%s", __func__ );
    return generic_command_common(t, "ATH\r", 90000);
}

command_result get_battery_status(CommandableIf *t, int &voltage, int &bcs, int &bcl)
{
    ESP_LOGV(TAG, "%s", __func__ );
    std::string_view out;
    auto ret = generic_get_string(t, "AT+CBC\r", out);
    if (ret != command_result::OK) {
        return ret;
    }

    constexpr std::string_view pattern = "+CBC: ";
    if (out.find(pattern) == std::string_view::npos) {
        return command_result::FAIL;
    }
    // Parsing +CBC: <bcs>,<bcl>,<voltage>
    out = out.substr(pattern.size());
    int pos, value, property = 0;
    while ((pos = out.find(',') != std::string::npos)) {
        if (std::from_chars(out.data(), out.data() + pos, value).ec == std::errc::invalid_argument) {
            return command_result::FAIL;
        }
        switch (property++) {
        case 0: bcs = value;
            break;
        case 1: bcl = value;
            break;
        default:
            return command_result::FAIL;
        }
        out = out.substr(pos + 1);
    }
    if (std::from_chars(out.data(), out.data() + out.size(), voltage).ec == std::errc::invalid_argument) {
        return command_result::FAIL;
    }
    return command_result::OK;
}

command_result get_battery_status_sim7xxx(CommandableIf *t, int &voltage, int &bcs, int &bcl)
{
    ESP_LOGV(TAG, "%s", __func__ );
    std::string_view out;
    auto ret = generic_get_string(t, "AT+CBC\r", out);
    if (ret != command_result::OK) {
        return ret;
    }
    // Parsing +CBC: <voltage in Volts> V
    constexpr std::string_view pattern = "+CBC: ";
    constexpr int num_pos = pattern.size();
    int dot_pos;
    if (out.find(pattern) == std::string::npos ||
            (dot_pos = out.find('.')) == std::string::npos) {
        return command_result::FAIL;
    }

    int volt, fraction;
    if (std::from_chars(out.data() + num_pos, out.data() + dot_pos, volt).ec == std::errc::invalid_argument) {
        return command_result::FAIL;
    }
    if (std::from_chars(out.data() + dot_pos + 1, out.data() + out.size() - 1, fraction).ec == std::errc::invalid_argument) {
        return command_result::FAIL;
    }
    bcl = bcs = -1; // not available for these models
    voltage = 1000 * volt + fraction;
    return command_result::OK;
}

command_result set_flow_control(CommandableIf *t, int dce_flow, int dte_flow)
{
    ESP_LOGV(TAG, "%s", __func__ );
    return generic_command_common(t, "AT+IFC=" + std::to_string(dce_flow) + ", " + std::to_string(dte_flow) + "\r");
}

command_result get_operator_name(CommandableIf *t, std::string &operator_name)
{
    ESP_LOGV(TAG, "%s", __func__ );
    std::string_view out;
    auto ret = generic_get_string(t, "AT+COPS?\r", out, 75000);
    if (ret != command_result::OK) {
        return ret;
    }
    auto pos = out.find("+COPS");
    auto property = 0;
    while (pos != std::string::npos) {
        // Looking for: +COPS: <mode>[, <format>[, <oper>]]
        if (property++ == 2) {  // operator name is after second comma (as a 3rd property of COPS string)
            operator_name = out.substr(++pos);
            return command_result::OK;
        }
        pos = out.find(',', ++pos);
    }
    return command_result::FAIL;
}

command_result set_echo(CommandableIf *t, bool on)
{
    ESP_LOGV(TAG, "%s", __func__ );
    if (on) {
        return generic_command_common(t, "ATE1\r");
    }
    return generic_command_common(t, "ATE0\r");
}

command_result set_pdp_context(CommandableIf *t, PdpContext &pdp)
{
    ESP_LOGV(TAG, "%s", __func__ );
    std::string pdp_command = "AT+CGDCONT=" + std::to_string(pdp.context_id) +
                              ",\"" + pdp.protocol_type + "\",\"" + pdp.apn + "\"\r";
    return generic_command_common(t, pdp_command);
}

command_result set_data_mode(CommandableIf *t)
{
    ESP_LOGV(TAG, "%s", __func__ );
    return generic_command(t, "ATD*99##\r", "CONNECT", "ERROR", 5000);
}

command_result set_data_mode_sim8xx(CommandableIf *t)
{
    ESP_LOGV(TAG, "%s", __func__ );
    return generic_command(t, "ATD*99##\r", "CONNECT", "ERROR", 5000);
}

command_result resume_data_mode(CommandableIf *t)
{
    ESP_LOGV(TAG, "%s", __func__ );
    return generic_command(t, "ATO\r", "CONNECT", "ERROR", 5000);
}

command_result set_command_mode(CommandableIf *t)
{
    ESP_LOGV(TAG, "%s", __func__ );
    const auto pass = std::list<std::string_view>({"NO CARRIER", "OK"});
    const auto fail = std::list<std::string_view>({"ERROR"});
    return generic_command(t, "+++", pass, fail, 5000);
}

command_result get_imsi(CommandableIf *t, std::string &imsi_number)
{
    ESP_LOGV(TAG, "%s", __func__ );
    return generic_get_string(t, "AT+CIMI\r", imsi_number, 5000);
}

command_result get_imei(CommandableIf *t, std::string &out)
{
    ESP_LOGV(TAG, "%s", __func__ );
    return generic_get_string(t, "AT+CGSN\r", out, 5000);
}

command_result get_module_name(CommandableIf *t, std::string &out)
{
    ESP_LOGV(TAG, "%s", __func__ );
    return generic_get_string(t, "AT+CGMM\r", out, 5000);
}

command_result sms_txt_mode(CommandableIf *t, bool txt = true)
{
    ESP_LOGV(TAG, "%s", __func__ );
    if (txt) {
        return generic_command_common(t, "AT+CMGF=1\r");    // Text mode (default)
    }
    return generic_command_common(t, "AT+CMGF=0\r");     // PDU mode
}

command_result sms_character_set(CommandableIf *t)
{
    // Sets the default GSM character set
    ESP_LOGV(TAG, "%s", __func__ );
    return generic_command_common(t, "AT+CSCS=\"GSM\"\r");
}

command_result send_sms(CommandableIf *t, const std::string &number, const std::string &message)
{
    ESP_LOGV(TAG, "%s", __func__ );
    auto ret = t->command("AT+CMGS=\"" + number + "\"\r", [&](uint8_t *data, size_t len) {
        std::string_view response((char *)data, len);
        ESP_LOGD(TAG, "Send SMS response %.*s", static_cast<int>(response.size()), response.data());
        if (response.find('>') != std::string::npos) {
            return command_result::OK;
        }
        return command_result::TIMEOUT;
    }, 5000, ' ');
    if (ret != command_result::OK) {
        return ret;
    }
    return generic_command_common(t, message + "\x1A", 120000);
}


command_result set_cmux(CommandableIf *t)
{
    ESP_LOGV(TAG, "%s", __func__ );
    return generic_command_common(t, "AT+CMUX=0\r");
}

command_result read_pin(CommandableIf *t, bool &pin_ok)
{
    ESP_LOGV(TAG, "%s", __func__ );
    std::string_view out;
    auto ret = generic_get_string(t, "AT+CPIN?\r", out);
    if (ret != command_result::OK) {
        return ret;
    }
    if (out.find("+CPIN:") == std::string::npos) {
        return command_result::FAIL;
    }
    if (out.find("SIM PIN") != std::string::npos || out.find("SIM PUK") != std::string::npos) {
        pin_ok = false;
        return command_result::OK;
    }
    if (out.find("READY") != std::string::npos) {
        pin_ok = true;
        return command_result::OK;
    }
    return command_result::FAIL; // Neither pin-ok, nor waiting for pin/puk -> mark as error
}

command_result set_pin(CommandableIf *t, const std::string &pin)
{
    ESP_LOGV(TAG, "%s", __func__ );
    std::string set_pin_command = "AT+CPIN=" + pin + "\r";
    return generic_command_common(t, set_pin_command);
}

command_result get_signal_quality(CommandableIf *t, int &rssi, int &ber)
{
    ESP_LOGV(TAG, "%s", __func__ );
    std::string_view out;
    auto ret = generic_get_string(t, "AT+CSQ\r", out);
    if (ret != command_result::OK) {
        return ret;
    }

    constexpr std::string_view pattern = "+CSQ: ";
    constexpr int rssi_pos = pattern.size();
    int ber_pos;
    if (out.find(pattern) == std::string::npos ||
            (ber_pos = out.find(',')) == std::string::npos) {
        return command_result::FAIL;
    }

    if (std::from_chars(out.data() + rssi_pos, out.data() + ber_pos, rssi).ec == std::errc::invalid_argument) {
        return command_result::FAIL;
    }
    if (std::from_chars(out.data() + ber_pos + 1, out.data() + out.size(), ber).ec == std::errc::invalid_argument) {
        return command_result::FAIL;
    }
    return command_result::OK;
}

} // esp_modem::dce_commands