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

#include <condition_variable>
#include <unistd.h>
#include "cxx_include/esp_modem_primitives.hpp"

namespace esp_modem {

struct SignalGroupInternal {
    std::condition_variable notify;
    std::mutex m;
    uint32_t flags{ 0 };
};


SignalGroup::SignalGroup(): event_group(std::make_unique<SignalGroupInternal>())
{
}

void SignalGroup::set(uint32_t bits)
{
    std::unique_lock<std::mutex> lock(event_group->m);
    event_group->flags |= bits;
}

void SignalGroup::clear(uint32_t bits)
{
    std::unique_lock<std::mutex> lock(event_group->m);
    event_group->flags &= ~bits;
}

bool SignalGroup::wait(uint32_t flags, uint32_t time_ms)
{
    std::unique_lock<std::mutex> lock(event_group->m);
    return event_group->notify.wait_for(lock, std::chrono::milliseconds(time_ms), [&]{
        if ((flags&event_group->flags) == flags) {
            event_group->flags &= ~flags;
            return true;
        }
        return false;
    });
//    return ret != std::cv_status::timeout;
//    , [&]{return flags&event_group->flags; });
}

bool SignalGroup::is_any(uint32_t flags)
{
    std::unique_lock<std::mutex> lock(event_group->m);
    return flags&event_group->flags;
}

bool SignalGroup::wait_any(uint32_t flags, uint32_t time_ms)
{
    std::unique_lock<std::mutex> lock(event_group->m);
    return event_group->notify.wait_for(lock, std::chrono::milliseconds(time_ms), [&]{ return flags&event_group->flags; });
}

SignalGroup::~SignalGroup() = default;

Task::Task(size_t stack_size, size_t priority, void *task_param, TaskFunction_t task_function)
{
#warning "Define this for linux"
}

Task::~Task()
{

}

void Task::Delete() {}

void Task::Relinquish()
{
    usleep(0);
}

} // namespace esp_modem