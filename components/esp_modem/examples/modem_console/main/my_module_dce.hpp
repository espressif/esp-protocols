/* Modem console example: Custom DCE

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#ifndef __MY_MODULE_DCE_HPP__
#define __MY_MODULE_DCE_HPP__


#include "cxx_include/esp_modem_dce_factory.hpp"
#include "cxx_include/esp_modem_dce_module.hpp"

/**
 * @brief Definition of a custom modem which inherits from the GenericModule, uses all its methods
 * and could override any of them. Here, for demonstration purposes only, we redefine just `get_module_name()`
 */
class MyShinyModem: public esp_modem::GenericModule {
    using GenericModule::GenericModule;
public:
    esp_modem::command_result get_module_name(std::string& name) override
    {
        name = "Custom Shiny Module";
        return esp_modem::command_result::OK;
    }
};

/**
 * @brief Helper create method which employs the DCE factory for creating DCE objects templated by a custom module
 * @return unique pointer of the resultant DCE
 */
std::unique_ptr<esp_modem::DCE> create_shiny_dce(const esp_modem::dce_config *config,
                                                 std::shared_ptr<esp_modem::DTE> dte,
                                                 esp_netif_t *netif)
{
    return esp_modem::dce_factory::Factory::build_unique<MyShinyModem>(config, std::move(dte), netif);
}


#endif //__MY_MODULE_DCE_HPP__