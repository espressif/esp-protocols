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

void Lock::unlock()
{
    xSemaphoreGiveRecursive(m);
}

Lock::Lock(): m(nullptr)
{
    m = xSemaphoreCreateRecursiveMutex();
    throw_if_false(m != nullptr, "create signal event group failed");
}

Lock::~Lock()
{
    vSemaphoreDelete(m);
}

void Lock::lock()
{
    xSemaphoreTakeRecursive(m, portMAX_DELAY);
}


SignalGroup::SignalGroup(): event_group(nullptr)
{
    event_group = xEventGroupCreate();
    throw_if_false(event_group != nullptr, "create signal event group failed");
}

void SignalGroup::set(uint32_t bits)
{
    xEventGroupSetBits(event_group, bits);
}

void SignalGroup::clear(uint32_t bits)
{
    xEventGroupClearBits(event_group, bits);
}

bool SignalGroup::wait(uint32_t flags, uint32_t time_ms)
{
    EventBits_t bits = xEventGroupWaitBits(event_group, flags, pdTRUE, pdTRUE, pdMS_TO_TICKS(time_ms));
    return bits & flags;
}

bool SignalGroup::is_any(uint32_t flags)
{
    return xEventGroupGetBits(event_group) & flags;
}

bool SignalGroup::wait_any(uint32_t flags, uint32_t time_ms)
{
    EventBits_t bits = xEventGroupWaitBits(event_group, flags, pdFALSE, pdFALSE, pdMS_TO_TICKS(time_ms));
    return bits & flags;
}

SignalGroup::~SignalGroup()
{
    if (event_group) {
        vEventGroupDelete(event_group);
    }
}

Task::Task(size_t stack_size, size_t priority, void *task_param, TaskFunction_t task_function)
    : task_handle(nullptr)
{
    BaseType_t ret = xTaskCreate(task_function, "vfs_task", stack_size, task_param, priority, &task_handle);
    throw_if_false(ret == pdTRUE, "create vfs task failed");
}

Task::~Task()
{
    if (task_handle) {
        vTaskDelete(task_handle);
    }
}

void Task::Delete()
{
    vTaskDelete(nullptr);
}

void Task::Relinquish()
{
    vTaskDelay(1);
}

void Task::Delay(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

} // namespace esp_modem
