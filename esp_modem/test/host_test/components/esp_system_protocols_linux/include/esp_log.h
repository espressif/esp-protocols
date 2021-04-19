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
