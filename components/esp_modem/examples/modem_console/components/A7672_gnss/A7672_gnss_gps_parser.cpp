/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
//
// Created on: 23.08.2022
// Author: franz

#include <cstring>
#include <string_view>
#include <charconv>
#include <list>
#include "sdkconfig.h"
#include "cxx_include/esp_modem_dte.hpp"
#include "cxx_include/esp_modem_dce.hpp"
#include "esp_modem_config.h"
#include "cxx_include/esp_modem_api.hpp"
#include "cxx_include/esp_modem_command_library_utils.hpp"
#include "esp_log.h"
#include "generate/esp_modem_command_declare.inc"
#include "A7672_gnss.hpp"

constexpr auto const TAG = "A7672_gnss";



esp_modem::command_result get_gps_information_a7672_lib(esp_modem::CommandableIf *t, a7672_gps_t &gps)
{

    ESP_LOGV(TAG, "%s", __func__ );
    std::string str_out;
    auto ret = esp_modem::dce_commands::generic_get_string(t, "AT+CGPSINFO\r", str_out);
    if (ret != esp_modem::command_result::OK) {
        return ret;
    }
    std::string_view out(str_out);


    constexpr std::string_view pattern = "+CGPSINFO: ";
    if (out.find(pattern) == std::string_view::npos) {
        return esp_modem::command_result::FAIL;
    }
    /**
     * Parsing +CGPSINFO:
        Defined Values
        | **Name** | **Example**  | **Unit** | **Description**                                               |
        |----------|--------------|----------|---------------------------------------------------------------|
        | lat      | 3113.343286  |          | Latitude of current position. Output format is ddmm.mmmmmm.   |
        | N/S      | N            |          | N/S Indicator, N=north or S=south.                            |
        | log      | 12121.234064 |          | Longitude of current position. Output format is dddmm.mmmmmm. |
        | E/W      | E            |          | E/W Indicator, E=east or W=west.                              |
        | date     | 250311       |          | Date. Output format is ddmmyy                                 |
        | UTC time | 072809.3     |          | UTC Time. Output format is hhmmss.s.                          |
        | alt      | 44.1         | meters   | MSL Altitude. Unit is meters.                                 |
        | speed    | 0.0          | knots    | Speed Over Ground. Unit is knots.                             |
        | course   | 0            | Degrees  | Course. Degrees.                                              |
     */
    out = out.substr(pattern.size());
    int pos = 0;
    if ((pos = out.find(',')) == std::string::npos) {
        return esp_modem::command_result::FAIL;
    }

    //Latitude
    {
        std::string_view Latitude = out.substr(0, pos);
        if (Latitude.length() > 1) {
            gps.latitude  = std::stof(std::string(Latitude));
        } else {
            gps.latitude  = 0;
        }
    } //clean up Latitude
    out = out.substr(pos + 1);
    if ((pos = out.find(',')) == std::string::npos) {
        return esp_modem::command_result::FAIL;
    }
    //Latitude N/S Indicator, N=north or S=south.
    {
        std::string_view Latitude_ns = out.substr(0, pos);
        if (Latitude_ns.length() > 1) {
            switch (std::string(Latitude_ns).c_str()[0]) {
            case 'N':
                gps.latitude_ns  = GPS_N;
                break;
            case 'S':
                gps.latitude_ns  = GPS_S;
                break;
            default:
                gps.latitude_ns  = GPS_NS_INVALID;
                break;
            }
        } else {
            gps.latitude_ns  = GPS_NS_INVALID;
        }
    } //clean up Latitude N/S Indicator, N=north or S=south.
    out = out.substr(pos + 1);
    if ((pos = out.find(',')) == std::string::npos) {
        return esp_modem::command_result::FAIL;
    }
    //Longitude
    {
        std::string_view Longitude = out.substr(0, pos);
        if (Longitude.length() > 1) {
            gps.longitude  = std::stof(std::string(Longitude));
        } else {
            gps.longitude  = 0;
        }
    } //clean up Longitude
    out = out.substr(pos + 1);
    if ((pos = out.find(',')) == std::string::npos) {
        return esp_modem::command_result::FAIL;
    }
    //Longitude E/W Indicator, E=east or W=west.
    {
        std::string_view Longitude_ew = out.substr(0, pos);
        if (Longitude_ew.length() > 1) {
            switch (std::string(Longitude_ew).c_str()[0]) {
            case 'E':
                gps.longitude_ew  = GPS_E;
                break;
            case 'W':
                gps.longitude_ew  = GPS_W;
                break;
            default:
                gps.longitude_ew  = GPS_EW_INVALID;
                break;
            }
        } else {
            gps.longitude_ew  = GPS_EW_INVALID;
        }
    } //clean up Longitude E/W Indicator, E=east or W=west.
    out = out.substr(pos + 1);
    if ((pos = out.find(',')) == std::string::npos) {
        return esp_modem::command_result::FAIL;
    }
    //UTC date
    {
        std::string_view UTC_date = out.substr(0, pos);
        if (UTC_date.length() > 1) {
            if (std::from_chars(out.data() + 0, out.data() + 2, gps.date.day).ec == std::errc::invalid_argument) {
                return esp_modem::command_result::FAIL;
            }
            if (std::from_chars(out.data() + 2, out.data() + 4, gps.date.month).ec == std::errc::invalid_argument) {
                return esp_modem::command_result::FAIL;
            }
            if (std::from_chars(out.data() + 4, out.data() + 6, gps.date.year).ec == std::errc::invalid_argument) {
                return esp_modem::command_result::FAIL;
            }
        } else {
            gps.date.year    = 0;
            gps.date.month   = 0;
            gps.date.day     = 0;
        }

    } //clean up UTC date
    out = out.substr(pos + 1);
    if ((pos = out.find(',')) == std::string::npos) {
        return esp_modem::command_result::FAIL;
    }
    //UTC Time
    {
        std::string_view UTC_time = out.substr(0, pos);
        if (UTC_time.length() > 1) {
            if (std::from_chars(out.data() + 0, out.data() + 2, gps.tim.hour).ec == std::errc::invalid_argument) {
                return esp_modem::command_result::FAIL;
            }
            if (std::from_chars(out.data() + 2, out.data() + 4, gps.tim.minute).ec == std::errc::invalid_argument) {
                return esp_modem::command_result::FAIL;
            }
            if (std::from_chars(out.data() + 4, out.data() + 6, gps.tim.second).ec == std::errc::invalid_argument) {
                return esp_modem::command_result::FAIL;
            }
            if (std::from_chars(out.data() + 7, out.data() + 8, gps.tim.thousand).ec == std::errc::invalid_argument) {
                return esp_modem::command_result::FAIL;
            }
        } else {
            gps.tim.hour     = 0;
            gps.tim.minute   = 0;
            gps.tim.second   = 0;
            gps.tim.thousand = 0;
        }

    } //clean up UTC Time
    out = out.substr(pos + 1);
    if ((pos = out.find(',')) == std::string::npos) {
        return esp_modem::command_result::FAIL;
    }
    //Altitude
    {
        std::string_view Altitude = out.substr(0, pos);
        if (Altitude.length() > 1) {
            gps.altitude  = std::stof(std::string(Altitude));
        } else {
            gps.altitude  = 0;
        }
    } //clean up Altitude
    out = out.substr(pos + 1);
    if ((pos = out.find(',')) == std::string::npos) {
        return esp_modem::command_result::FAIL;
    }
    //Speed Over Ground knots
    {
        std::string_view gps_speed = out.substr(0, pos);
        if (gps_speed.length() > 1) {
            gps.speed  = std::stof(std::string(gps_speed));
        } else {
            gps.speed  = 0;
        }
    } //clean up Speed Over Ground knots
    out = out.substr(pos + 1);
    if ((pos = out.find(',')) == std::string::npos) {
        return esp_modem::command_result::FAIL;
    }
    //Course Over Ground degrees
    {
        std::string_view gps_cog = out.substr(0, pos);
        if (gps_cog.length() > 1) {
            gps.cog  = std::stof(std::string(gps_cog));
        } else {
            gps.cog  = 0;
        }
    } //clean up gps_cog
    out = out.substr(pos + 1);
    if ((pos = out.find(',')) == std::string::npos) {
        return esp_modem::command_result::FAIL;
    }
    return esp_modem::command_result::OK;
}


esp_modem::command_result A7672_gnss::get_gps_information_a7672(a7672_gps_t &gps)
{
    return get_gps_information_a7672_lib(dte.get(), gps);
}


esp_modem::command_result A7672::DCE_gnss::get_gps_information_a7672(a7672_gps_t &gps)
{
    return device->get_gps_information_a7672(gps);
}
