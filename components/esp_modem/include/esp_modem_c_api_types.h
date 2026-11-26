/*
 * SPDX-FileCopyrightText: 2021-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "sdkconfig.h"
#include "esp_modem_config.h"
#include "esp_netif.h"

#ifdef __cplusplus
extern "C" {
#endif

/*  Compatibility macro to be removed in v2.0
 */
#define ESP_MODEM_DCE_GENETIC ESP_MODEM_DCE_GENERIC

typedef struct esp_modem_dce_wrap esp_modem_dce_t;

/**
 * @brief PDP context for AT+CGDCONT / network attach
 *
 * @note protocol_type: "IP" (IPv4), "IPV6", or "IPV4V6" (dual stack).
 *       context_id: typically 1; some carriers (e.g. Verizon) use 3.
 *       Zero context_id / NULL protocol_type keep defaults (1 / "IP"),
 *       matching esp_modem_dce_config.
 */
typedef struct esp_modem_PdpContext_t {
    size_t context_id;          /*!< PDP context id (CID); 0 keeps default 1 */
    const char *protocol_type;  /*!< "IP", "IPV6", or "IPV4V6"; NULL keeps default "IP" */
    const char *apn;            /*!< Access Point Name */
} esp_modem_PdpContext_t;

/**
 * @brief Minimum byte size of caller-allocated buffers for C-API string output parameters.
 *
 * Buffers passed to esp_modem_get_imsi(), esp_modem_get_imei(),
 * esp_modem_get_operator_name(), esp_modem_get_module_name(), esp_modem_at(), and
 * esp_modem_at_raw() must be at least this many bytes (including space for a NUL terminator).
 */
#define ESP_MODEM_C_API_STR_BUF_SIZE CONFIG_ESP_MODEM_C_API_STR_MAX

/**
 * @brief SIM PIN status reported by AT+CPIN?
 */
typedef enum esp_modem_sim_pin_state {
    ESP_MODEM_SIM_PIN_STATE_UNKNOWN = 0,
    ESP_MODEM_SIM_PIN_STATE_READY,
    ESP_MODEM_SIM_PIN_STATE_NEED_PIN,
    ESP_MODEM_SIM_PIN_STATE_NEED_PUK,
    ESP_MODEM_SIM_PIN_STATE_OTHER,
} esp_modem_sim_pin_state_t;

/**
 * @defgroup ESP_MODEM_C_API ESP_MODEM C API
 * @brief Set of basic C API for ESP-MODEM
 */
/** @addtogroup ESP_MODEM_C_API
 * @{
 */

/**
 * @brief DCE mode: This enum is used to set desired operation mode of the DCE
 */
typedef enum esp_modem_dce_mode {
    ESP_MODEM_MODE_COMMAND,  /**< Default mode after modem startup, used for sending AT commands */
    ESP_MODEM_MODE_DATA,     /**< Used for switching to PPP mode for the modem to connect to a network */
    ESP_MODEM_MODE_CMUX,     /**< Multiplexed terminal mode */
    ESP_MODEM_MODE_CMUX_MANUAL,         /**< CMUX manual mode */
    ESP_MODEM_MODE_CMUX_MANUAL_EXIT,    /**< Exit CMUX manual mode */
    ESP_MODEM_MODE_CMUX_MANUAL_SWAP,    /**< Swap terminals in CMUX manual mode */
    ESP_MODEM_MODE_CMUX_MANUAL_DATA,    /**< Set DATA mode in CMUX manual mode */
    ESP_MODEM_MODE_CMUX_MANUAL_COMMAND, /**< Set COMMAND mode in CMUX manual mode */
    ESP_MODEM_MODE_DETECT,              /**< Detect the mode and resume it (if sucessfully detected) */
    ESP_MODEM_MODE_UNDEF,
} esp_modem_dce_mode_t;

/**
 * @brief DCE devices: Enum list of supported devices
 */
typedef enum esp_modem_dce_device {
    ESP_MODEM_DCE_GENERIC,  /**< The most generic device */
    ESP_MODEM_DCE_SIM7600,
    ESP_MODEM_DCE_SIM7070,
    ESP_MODEM_DCE_SIM7000,
    ESP_MODEM_DCE_BG96,
    ESP_MODEM_DCE_EC20,
    ESP_MODEM_DCE_SIM800,
    ESP_MODEM_DCE_SQNGM02S,
    ESP_MODEM_DCE_CUSTOM
} esp_modem_dce_device_t;

/**
 * @brief Terminal errors
 */
typedef enum esp_modem_terminal_error {
    ESP_MODEM_TERMINAL_BUFFER_OVERFLOW,
    ESP_MODEM_TERMINAL_CHECKSUM_ERROR,
    ESP_MODEM_TERMINAL_UNEXPECTED_CONTROL_FLOW,
    ESP_MODEM_TERMINAL_DEVICE_GONE,
    ESP_MODEM_TERMINAL_UNKNOWN_ERROR,
} esp_modem_terminal_error_t;

/**
 * @brief Terminal error callback
 */
typedef void (*esp_modem_terminal_error_cbt)(esp_modem_terminal_error_t);

/**
 * @brief Create a generic DCE handle for new modem API
 *
 * @param dte_config DTE configuration (UART config for now)
 * @param dce_config DCE configuration
 * @param netif Network interface handle for the data mode
 *
 * @return DCE pointer on success, NULL on failure
 */
esp_modem_dce_t *esp_modem_new(const esp_modem_dte_config_t *dte_config, const esp_modem_dce_config_t *dce_config, esp_netif_t *netif);

/**
 * @brief Create a DCE handle using the supplied device
 *
 * @param module Specific device for creating this DCE
 * @param dte_config DTE configuration (UART config for now)
 * @param dce_config DCE configuration
 * @param netif Network interface handle for the data mode
 *
 * @return DCE pointer on success, NULL on failure
 */
esp_modem_dce_t *esp_modem_new_dev(esp_modem_dce_device_t module, const esp_modem_dte_config_t *dte_config, const esp_modem_dce_config_t *dce_config, esp_netif_t *netif);

/**
 * @brief Destroys modem's DCE handle
 *
 * @param dce DCE to destroy
 */
void esp_modem_destroy(esp_modem_dce_t *dce);

/**
 * @brief Set DTE's error callback
 *
 * @param dce Modem DCE handle
 * @param[in] err_cb Error callback
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t esp_modem_set_error_cb(esp_modem_dce_t *dce, esp_modem_terminal_error_cbt err_cb);

/**
 * @brief Set operation mode for this DCE
 * @param dce Modem DCE handle
 * @param mode Desired MODE
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t esp_modem_set_mode(esp_modem_dce_t *dce, esp_modem_dce_mode_t mode);

/**
 * @brief Convenient function to run arbitrary commands from C-API
 *
 * @param dce Modem DCE handle
 * @param command Command to send
 * @param got_line_cb Callback function which is called whenever we receive a line
 * @param timeout_ms Command timeout
 * @return ESP_OK on success, ESP_FAIL on failure
 */

esp_err_t esp_modem_command(esp_modem_dce_t *dce, const char *command, esp_err_t(*got_line_cb)(uint8_t *data, size_t len), uint32_t timeout_ms);

/**
 * @brief Sets the APN on the stored PDP context (keeps protocol type and CID)
 *
 * @param dce Modem DCE handle
 * @param apn Access Point Name
 * @return ESP_OK on success
 *
 * @note To change protocol type or CID as well, use esp_modem_set_pdp_context().
 */
esp_err_t esp_modem_set_apn(esp_modem_dce_t *dce, const char *apn);

#ifdef CONFIG_ESP_MODEM_URC_HANDLER
/**
 * @brief Sets a handler for unsolicited result codes (URCs) from the modem
 *
 * This function registers a callback that is triggered whenever an unsolicited
 * result code (URC) is received from the modem. URCs are typically sent by the
 * modem without a prior command to notify the host about certain events or status changes.
 *
 * @param dce Modem DCE handle
 * @param got_line_cb Callback function which is called whenever a URC line is received
 * @return ESP_OK on success, ESP_FAIL on failure
 */
esp_err_t esp_modem_set_urc(esp_modem_dce_t *dce, esp_err_t(*got_line_cb)(uint8_t *data, size_t len));

/**
 * @brief Command-channel buffer state handed to an enhanced URC callback
 *
 * Mirrors DTE::UrcBufferInfo for C callers. The callback runs before the
 * active command's own parser sees the same bytes.
 */
typedef struct {
    const uint8_t *buffer_start;     /*!< Start of the accumulated command buffer */
    size_t buffer_total_size;        /*!< Bytes currently in the buffer */
    size_t processed_offset;         /*!< Bytes already consumed by earlier callbacks */
    size_t new_data_size;            /*!< Bytes added since the previous callback */
    const uint8_t *new_data_start;   /*!< Start of those bytes */
    bool is_command_active;          /*!< A command is waiting for its reply */
} esp_modem_urc_buffer_info_t;

typedef enum {
    ESP_MODEM_URC_CONSUME_NONE,      /*!< Leave the buffer to the active command */
    ESP_MODEM_URC_CONSUME_PARTIAL,   /*!< Consume consume_size leading bytes, the active command sees the rest */
    ESP_MODEM_URC_CONSUME_ALL,       /*!< Consume the whole buffer, including any command reply in it */
} esp_modem_urc_consume_result_t;

typedef struct {
    esp_modem_urc_consume_result_t result;
    size_t consume_size;             /*!< Bytes to consume for CONSUME_PARTIAL */
} esp_modem_urc_consume_info_t;

/**
 * @brief Set the enhanced URC callback with buffer consumption control
 *
 * C entry for DTE::set_enhanced_urc_cb(). The callback decides how much of
 * the command buffer it consumed; returning ESP_MODEM_URC_CONSUME_NONE while
 * a command is active leaves the reply to that command. Pass NULL to detach.
 *
 * While is_command_active is set, the buffer may hold the reply of that
 * command next to the URC. ESP_MODEM_URC_CONSUME_ALL discards it as well and
 * the command then times out: consume only complete URC lines with
 * ESP_MODEM_URC_CONSUME_PARTIAL, or return ESP_MODEM_URC_CONSUME_NONE.
 *
 * @param dce Modem DCE handle
 * @param urc_cb Callback receiving the buffer state and returning the consumption
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG on a bad handle
 */
esp_err_t esp_modem_set_enhanced_urc(esp_modem_dce_t *dce,
                                     esp_modem_urc_consume_info_t (*urc_cb)(const esp_modem_urc_buffer_info_t *info));

#endif

/**
 * @brief Transmit hook callback type
 * @param user_ctx Opaque context pointer registered with esp_modem_set_transmit_hooks()
 */
typedef void (*esp_modem_transmit_hook_t)(void *user_ctx);

/**
 * @brief Register hooks that fire around every DTE write operation
 *
 * Allows toggling a GPIO (DTR, sleep pin, etc.) before and after UART writes,
 * covering both application-initiated AT commands and internal PPP traffic.
 *
 * @param dce       Modem DCE handle
 * @param before_tx Called just before bytes are sent (NULL to clear)
 * @param after_tx  Called just after bytes are sent (NULL to clear)
 * @param user_ctx  Opaque pointer forwarded to both hooks
 * @return ESP_OK on success, ESP_ERR_INVALID_ARG if dce is invalid
 */
esp_err_t esp_modem_set_transmit_hooks(esp_modem_dce_t *dce,
                                       esp_modem_transmit_hook_t before_tx,
                                       esp_modem_transmit_hook_t after_tx,
                                       void *user_ctx);

/**
 * @brief Writes raw bytes to the modem's command channel
 *
 * The C API otherwise carries only NUL-terminated command strings. A
 * prompt-driven upload, such as the payload after AT+CCHSEND's '>', needs
 * arbitrary bytes. The DCE must be in command mode (or CMUX manual command
 * mode); the caller sequences the prompt and the final result through
 * esp_modem_command().
 *
 * @param dce Modem DCE handle
 * @param data Bytes to write
 * @param len Number of bytes
 * @return ESP_OK when every byte was accepted by the DTE, ESP_FAIL otherwise
 */
esp_err_t esp_modem_write(esp_modem_dce_t *dce, const uint8_t *data, size_t len);

esp_err_t esp_modem_sqn_gm02s_connect(esp_modem_dce_t *dce, const esp_modem_PdpContext_t *pdp_context);

/**
 * @brief This API provides support for temporarily pausing networking in order
 * to send/receive AT commands and resume networking afterwards.
 * @note This function does not switch modes, the modem is still in data mode.
 *
 * @param dce Modem DCE handle
 * @param pause true to pause the network interface, false to resume networking
 * @return ESP_OK on success
 */
esp_err_t esp_modem_pause_net(esp_modem_dce_t *dce, bool pause);

esp_modem_dce_mode_t esp_modem_get_mode(esp_modem_dce_t *dce);
/**
 * @}
 */

#ifdef __cplusplus
}
#endif
