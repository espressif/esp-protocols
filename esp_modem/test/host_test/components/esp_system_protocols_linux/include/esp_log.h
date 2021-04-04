//
// Created by david on 2/10/21.
//

#ifndef MDNS_HOST_ESP_LOG_H
#define MDNS_HOST_ESP_LOG_H

#include <stdio.h>

#define ESP_LOG_INFO 1
#define ESP_LOG_BUFFER_HEXDUMP(...)

#define ESP_LOGE(TAG, ...)
//printf(TAG); printf("ERROR: " __VA_ARGS__); printf("\n")
#define ESP_LOGW(TAG, ...)
//printf(TAG); printf("WARN: "__VA_ARGS__); printf("\n")
#define ESP_LOGI(TAG, ...)
//printf(TAG); printf("INFO: "__VA_ARGS__); printf("\n")
#define ESP_LOGD(TAG, ...)
//printf(TAG); printf("DEBUG: "__VA_ARGS__); printf("\n")

#endif //MDNS_HOST_ESP_LOG_H
