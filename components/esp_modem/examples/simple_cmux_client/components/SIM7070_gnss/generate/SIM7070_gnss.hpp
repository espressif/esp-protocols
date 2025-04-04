/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
//
// Created on: 23.08.2022
// Author: franz


#pragma once


#include "cxx_include/esp_modem_dce_factory.hpp"
#include "cxx_include/esp_modem_dce_module.hpp"
#include "cxx_include/esp_modem_types.hpp"

#include "sim70xx_gps.h"

using PdpContext = esp_modem::PdpContext;

/**
 * @brief Definition of a custom SIM7070 class with GNSS capabilities.
 * This inherits from the official esp-modem's SIM7070 device which contains all common library methods.
 * On top of that, the SIM7070_gnss adds reading GNSS information, which is implemented in a private component.
 */
class SIM7070_gnss: public esp_modem::SIM7070 {
    using SIM7070::SIM7070;
public:
    esp_modem::command_result get_gnss_information_sim70xx(sim70xx_gps_t &gps);
};

/**
 * @brief DCE for the SIM7070_gnss. Here we've got to forward the general commands, aa well as the GNSS one.
 */
class DCE_gnss : public esp_modem::DCE_T<SIM7070_gnss> {
public:

    using DCE_T<SIM7070_gnss>::DCE_T;

//  --- ESP-MODEM command module starts here ---
#include "esp_modem_command_declare_helper.inc"
#define ESP_MODEM_DECLARE_DCE_COMMAND(name, return_type, ...) \
     esp_modem::return_type name(ESP_MODEM_COMMAND_PARAMS(__VA_ARGS__)) { return device->name(ESP_MODEM_COMMAND_FORWARD(__VA_ARGS__)); }

#include "esp_modem_command_declare.inc"
#undef ESP_MODEM_DECLARE_DCE_COMMAND


    esp_modem::command_result get_gnss_information_sim70xx(sim70xx_gps_t &gps);

    esp_modem::command_result get_operator_name(std::string &name)
    {
        return device->get_operator_name(name);
    }
};


/**
 * @brief Helper create method which employs the customized DCE factory for building DCE_gnss objects
 * @return unique pointer of the specific DCE
 */
std::unique_ptr<DCE_gnss> create_SIM7070_GNSS_dce(const esp_modem::dce_config *config,
                                                  std::shared_ptr<esp_modem::DTE> dte,
                                                  esp_netif_t *netif);
