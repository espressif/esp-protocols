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
//
// Created on: 23.08.2022
// Author: franz


#pragma once


#include "cxx_include/esp_modem_dce_factory.hpp"
#include "cxx_include/esp_modem_dce_module.hpp"
#include "nmea_parser.h"

/**
 * @brief Definition of a custom modem which inherits from the GenericModule, uses all its methods
 * and could override any of them. Here, for demonstration purposes only, we redefine just `get_module_name()`
 */
class SIM7070_gnss: public esp_modem::SIM7070 {
    using SIM7070::SIM7070;
public:
    esp_modem::command_result get_gnss_information_sim70xx(gps_t& gps);
};

class DCE_gnss : public esp_modem::DCE_T<SIM7070_gnss> {
public:

    using DCE_T<SIM7070_gnss>::DCE_T;
#define ESP_MODEM_DECLARE_DCE_COMMAND(name, return_type, num, ...) \
    template <typename ...Agrs> \
    esp_modem::return_type name(Agrs&&... args)   \
    {   \
        return device->name(std::forward<Agrs>(args)...); \
    }

    DECLARE_ALL_COMMAND_APIS(forwards name(...)
                             {
                                 device->name(...);
                             } )

#undef ESP_MODEM_DECLARE_DCE_COMMAND

    esp_modem::command_result get_gnss_information_sim70xx(gps_t& gps);

};


/**
 * @brief Helper create method which employs the DCE factory for creating DCE objects templated by a custom module
 * @return unique pointer of the resultant DCE
 */
std::unique_ptr<DCE_gnss> create_SIM7070_GNSS_dce(const esp_modem::dce_config *config,
        std::shared_ptr<esp_modem::DTE> dte,
        esp_netif_t *netif);
