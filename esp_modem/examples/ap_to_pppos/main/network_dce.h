/*  softAP to PPPoS Example (network_dce)

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#ifndef _NETWORK_DCE_H_
#define _NETWORK_DCE_H_

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @brief Initialize a singleton covering the PPP network provided by the connected modem device
 *
 * @param netif Already created network interface in PPP mode
 *
 * @return ESP_OK on success
 */
esp_err_t modem_init_network(esp_netif_t *netif);

/**
 * @brief Destroys the single network DCE
 */
void modem_deinit_network();

/**
 * @brief Starts the PPP network
 */
void modem_start_network();

/**
 * @brief Stops the PPP network
 */
void modem_stop_network();


#ifdef __cplusplus
}
#endif

#endif //_NETWORK_DCE_H_
