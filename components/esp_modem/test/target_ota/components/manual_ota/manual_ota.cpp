/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include "manual_ota.hpp"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_app_format.h"
#include "esp_http_client.h"
#include "esp_partition.h"
#include "esp_transport_tcp.h"
#include "transport_batch_tls.hpp"

static const char *TAG = "manual_ota";

bool manual_ota::begin()
{
    if (status != state::UNDEF) {
        ESP_LOGE(TAG, "Invalid state");
        return false;
    }
    status = state::INIT;
    esp_transport_handle_t tcp = esp_transport_tcp_init();
    ssl_ = esp_transport_batch_tls_init(tcp, max_buffer_size_);

    esp_http_client_config_t config = { };
    config.skip_cert_common_name_check = true;
    config.url = uri_;
    config.transport = ssl_;
    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();

    if (configured != running) {
        ESP_LOGE(TAG, "Configured OTA boot partition at offset 0x%08" PRIx32 ", but running from offset 0x%08" PRIx32, configured->address, running->address);
        return false;
    }

    http_ = esp_http_client_init(&config);
    if (http_ == nullptr) {
        ESP_LOGE(TAG, "Failed to initialise HTTP connection");
        return false;
    }
    esp_http_client_set_method(http_, HTTP_METHOD_HEAD);
    esp_err_t err = esp_http_client_perform(http_);
    if (err == ESP_OK) {
        int http_status = esp_http_client_get_status_code(http_);
        if (http_status != HttpStatus_Ok) {
            ESP_LOGE(TAG, "Received incorrect http status %d", http_status);
            return false;
        }
    } else {
        ESP_LOGE(TAG, "ESP HTTP client perform failed: %d", err);
        return false;
    }
    image_length_ = esp_http_client_get_content_length(http_);
    ESP_LOGI(TAG, "image_length = %lld", image_length_);
    esp_http_client_close(http_);

    if (image_length_ > size_) {
        char *header_val = nullptr;
        asprintf(&header_val, "bytes=0-%d", max_buffer_size_ - 1);
        if (header_val == nullptr) {
            ESP_LOGE(TAG, "Failed to allocate memory for HTTP header");
            return false;
        }
        esp_http_client_set_header(http_, "Range", header_val);
        free(header_val);
    }
    esp_http_client_set_method(http_, HTTP_METHOD_GET);

    partition_ = esp_ota_get_next_update_partition(nullptr);
    if (partition_ == nullptr) {
        ESP_LOGE(TAG, "Invalid update partition");
        return false;
    }
    ESP_LOGI(TAG, "Writing to partition subtype %d at offset 0x%" PRIx32, partition_->subtype, partition_->address);

    file_length_ = 0;
    reconnect_attempts_ = 0;
    buffer_.resize(max_buffer_size_);
    status = state::IMAGE_CHECK;
    return true;
}

bool manual_ota::perform()
{
    if (status != state::IMAGE_CHECK && status != state::START) {
        ESP_LOGE(TAG, "Invalid state");
        return false;
    }
    esp_err_t err = esp_http_client_open(http_, 0);
    if (err != ESP_OK) {
        if (image_length_ == file_length_) {
            status = state::END;
            return false;
        }

        esp_http_client_close(http_);
        ESP_LOGI(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        if (reconnect_attempts_++ < max_reconnect_attempts_) {
            if (prepare_reconnect()) {
                return true;    // will retry in the next iteration
            }
        }
        return fail_cleanup();
    }
    esp_http_client_fetch_headers(http_);

    int batch_len = esp_transport_batch_tls_pre_read(ssl_, max_buffer_size_, timeout_ * 1000);
    if (batch_len < 0) {
        ESP_LOGE(TAG, "Error: Failed to pre-read plain text data!");
        return fail_cleanup();
    }

    int data_read = esp_http_client_read(http_, buffer_.data(), batch_len);

    if (data_read < 0) {
        ESP_LOGE(TAG, "Error: SSL data read error");
        return fail_cleanup();
    } else if (data_read > 0) {
        esp_http_client_close(http_);

        if (status == state::IMAGE_CHECK) {
            esp_app_desc_t new_app_info;
            if (data_read > sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t)) {
                // check current version with downloading
                memcpy(&new_app_info, &buffer_[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)], sizeof(esp_app_desc_t));
                ESP_LOGI(TAG, "New firmware version: %s", new_app_info.version);

                esp_app_desc_t running_app_info;
                const esp_partition_t *running = esp_ota_get_running_partition();
                if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) {
                    ESP_LOGI(TAG, "Running firmware version: %s", running_app_info.version);
                }

                const esp_partition_t *last_invalid_app = esp_ota_get_last_invalid_partition();
                esp_app_desc_t invalid_app_info;
                if (esp_ota_get_partition_description(last_invalid_app, &invalid_app_info) == ESP_OK) {
                    ESP_LOGI(TAG, "Last invalid firmware version: %s", invalid_app_info.version);
                }

                // check current version with last invalid partition
                if (last_invalid_app != NULL) {
                    if (memcmp(invalid_app_info.version, new_app_info.version, sizeof(new_app_info.version)) == 0) {
                        ESP_LOGW(TAG, "New version is the same as invalid version.");
                        ESP_LOGW(TAG, "Previously, there was an attempt to launch the firmware with %s version, but it failed.", invalid_app_info.version);
                        ESP_LOGW(TAG, "The firmware has been rolled back to the previous version.");
                        return fail_cleanup();
                    }
                }

                status = state::START;
                err = esp_ota_begin(partition_, OTA_WITH_SEQUENTIAL_WRITES, &update_handle_);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
                    esp_ota_abort(update_handle_);
                    return fail_cleanup();
                }
                ESP_LOGI(TAG, "esp_ota_begin succeeded");
            } else {
                ESP_LOGE(TAG, "Received chunk doesn't contain app descriptor");
                esp_ota_abort(update_handle_);
                return fail_cleanup();
            }
        }
        err = esp_ota_write(update_handle_, (const void *)buffer_.data(), data_read);
        if (err != ESP_OK) {
            esp_ota_abort(update_handle_);
            return fail_cleanup();
        }
        file_length_ += data_read;
        ESP_LOGI(TAG, "Written image length %d", file_length_);

        if (image_length_ == file_length_) {
            status = state::END;
            return false;
        }

        if (!prepare_reconnect()) {
            esp_ota_abort(update_handle_);
            return fail_cleanup();
        }

    } else if (data_read == 0) {
        if (file_length_ == 0) {
            // Try to handle possible HTTP redirections
            int status_code = esp_http_client_get_status_code(http_);
            ESP_LOGW(TAG, "Status code: %d", status_code);
            err = esp_http_client_set_redirection(http_);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "URL redirection Failed");
                esp_ota_abort(update_handle_);
                return fail_cleanup();
            }

            err = esp_http_client_open(http_, 0);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
                return fail_cleanup();
            }
            esp_http_client_fetch_headers(http_);
        }
    }

    return true;
}

bool manual_ota::prepare_reconnect()
{
    esp_http_client_set_method(http_, HTTP_METHOD_GET);
    char *header_val = nullptr;
    if ((image_length_ - file_length_) > max_buffer_size_) {
        asprintf(&header_val, "bytes=%d-%d", file_length_, (file_length_ + max_buffer_size_ - 1));
    } else {
        asprintf(&header_val, "bytes=%d-", file_length_);
    }
    if (header_val == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate memory for HTTP header");
        return false;
    }
    esp_http_client_set_header(http_, "Range", header_val);
    free(header_val);
    return true;
}

bool manual_ota::fail_cleanup()
{
    esp_http_client_close(http_);
    esp_http_client_cleanup(http_);
    status = state::FAIL;
    return false;
}

bool manual_ota::end()
{
    if (status == state::END) {
        if (!esp_http_client_is_complete_data_received(http_)) {
            ESP_LOGE(TAG, "Error in receiving complete file");
            return fail_cleanup();
        }
        esp_err_t err = esp_ota_end(update_handle_);
        if (err != ESP_OK) {
            if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
                ESP_LOGE(TAG, "Image validation failed, image is corrupted");
            } else {
                ESP_LOGE(TAG, "esp_ota_end failed (%s)!", esp_err_to_name(err));
            }
            return fail_cleanup();
        }
        err = esp_ota_set_boot_partition(partition_);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
            return fail_cleanup();
        }
        return true;
    }
    return false;
}
