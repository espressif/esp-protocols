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

#pragma once

#include <memory>
#include "cxx_include/esp_modem_dce.hpp"
#include "cxx_include/esp_modem_dce_module.hpp"

struct esp_modem_dte_config;
struct esp_modem_dce_config;


namespace esp_modem {

class DTE;
typedef struct esp_netif_obj esp_netif_t;


/**
 * @defgroup ESP_MODEM_INIT_DTE ESP_MODEM Initialization API for DTE
 * @brief Create DTE's
 */
/** @addtogroup ESP_MODEM_INIT_DTE
* @{
*/

using dce_config = ::esp_modem_dce_config;
using dte_config = ::esp_modem_dte_config;

/**
 * @brief Create UART DTE
 * @param config DTE configuration
 * @return shared ptr to DTE on success
 *         nullptr on failure (either due to insufficient memory or wrong dte configuration)
 *         if exceptions are disabled the API abort()'s on error
 */
std::shared_ptr<DTE> create_uart_dte(const dte_config *config);

/**
 * @brief Create VFS DTE
 * @param config DTE configuration
 * @return shared ptr to DTE on success
 *         nullptr on failure (either due to insufficient memory or wrong dte configuration)
 *         if exceptions are disabled the API abort()'s on error
 */
std::shared_ptr<DTE> create_vfs_dte(const dte_config *config);


/**
 * @}
 */

/**
 * @defgroup ESP_MODEM_INIT_DCE ESP_MODEM Initialization API for DCE
 * @brief ESP_MODEM Initialization API for DCE
 */
/** @addtogroup ESP_MODEM_INIT_DCE
* @{
*/

/**
 * @brief Create DCE based on SIM7600 module
 * @param config DCE configuration
 * @param dte reference to the communicating DTE
 * @param netif reference to the network interface
 *
 * @return unique ptr to the created DCE on success
 *         nullptr on failure
 *         if exceptions are disabled the API abort()'s on error
 */
std::unique_ptr<DCE> create_SIM7600_dce(const dce_config *config, std::shared_ptr<DTE> dte, esp_netif_t *netif);

/**
 * @brief Create DCE based on SIM7070 module
 */
std::unique_ptr<DCE> create_SIM7070_dce(const dce_config *config, std::shared_ptr<DTE> dte, esp_netif_t *netif);

/**
 * @brief Create DCE based on SIM7000 module
 */
std::unique_ptr<DCE> create_SIM7000_dce(const dce_config *config, std::shared_ptr<DTE> dte, esp_netif_t *netif);


/**
 * @brief Create DCE based on SIM800 module
 */
std::unique_ptr<DCE> create_SIM800_dce(const dce_config *config, std::shared_ptr<DTE> dte, esp_netif_t *netif);

/**
 * @brief Create DCE based on BG96 module
 */
std::unique_ptr<DCE> create_BG96_dce(const dce_config *config, std::shared_ptr<DTE> dte, esp_netif_t *netif);

/**
 * @brief Create generic DCE
 */
std::unique_ptr<DCE> create_generic_dce(const dce_config *config, std::shared_ptr<DTE> dte, esp_netif_t *netif);

/**
 * @}
 */

} // namespace esp_modem
