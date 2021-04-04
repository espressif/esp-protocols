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

#include "cxx_include/esp_modem_primitives.hpp"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"



namespace esp_modem {



void Lock::unlock() {
    xSemaphoreGiveRecursive(m);
}

Lock::Lock(): m(nullptr)
{
    m = xSemaphoreCreateRecursiveMutex();
    throw_if_false(m != nullptr, "create signal event group failed");
}

Lock::~Lock() {
    vSemaphoreDelete(m);
}

void Lock::lock() {
    xSemaphoreTakeRecursive(m, portMAX_DELAY);
}


signal_group::signal_group(): event_group(nullptr)
{
    event_group = xEventGroupCreate();
    throw_if_false(event_group != nullptr, "create signal event group failed");
}

void signal_group::set(uint32_t bits)
{
    xEventGroupSetBits(event_group, bits);
}

void signal_group::clear(uint32_t bits)
{
    xEventGroupClearBits(event_group, bits);
}

bool signal_group::wait(uint32_t flags, uint32_t time_ms)
{
    EventBits_t bits = xEventGroupWaitBits(event_group, flags, pdTRUE, pdTRUE, pdMS_TO_TICKS(time_ms));
    return bits & flags;
}

bool signal_group::is_any(uint32_t flags)
{
    return xEventGroupGetBits(event_group) & flags;
}

bool signal_group::wait_any(uint32_t flags, uint32_t time_ms)
{
    EventBits_t bits = xEventGroupWaitBits(event_group, flags, pdFALSE, pdFALSE, pdMS_TO_TICKS(time_ms));
    return bits & flags;
}

signal_group::~signal_group()
{
    if (event_group) vEventGroupDelete(event_group);
}

} // namespace esp_modem