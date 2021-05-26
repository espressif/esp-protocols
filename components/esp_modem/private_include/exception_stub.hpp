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

#ifdef CONFIG_COMPILER_CXX_EXCEPTIONS
#define TRY_CATCH_OR_DO(block, action) \
    try { block                   \
    } catch (std::bad_alloc& e) { \
        ESP_LOGE(TAG, "Out of memory"); \
        action; \
    } catch (::esp_modem::esp_err_exception& e) {    \
        esp_err_t err = e.get_err_t();  \
        ESP_LOGE(TAG, "%s: Exception caught with ESP err_code=%d", __func__, err); \
        ESP_LOGE(TAG, "%s", e.what());  \
        action; \
    }

#define TRY_CATCH_RET_NULL(block) TRY_CATCH_OR_DO(block, return nullptr)
#else

#define TRY_CATCH_OR_DO(block, action) \
    block

#define TRY_CATCH_RET_NULL(block) \
    block

#endif
