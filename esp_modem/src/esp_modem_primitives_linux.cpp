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
#include "cxx_include/esp_modem_primitives.hpp"


namespace esp_modem {

struct SignalGroup {
    std::condition_variable notify;
    std::mutex m;
    uint32_t flags{ 0 };
};


signal_group::signal_group(): event_group(std::make_unique<SignalGroup>())
{
}

void signal_group::set(uint32_t bits)
{
    std::unique_lock<std::mutex> lock(event_group->m);
    event_group->flags |= bits;
}

void signal_group::clear(uint32_t bits)
{
    std::unique_lock<std::mutex> lock(event_group->m);
    event_group->flags &= ~bits;
}

bool signal_group::wait(uint32_t flags, uint32_t time_ms)
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

bool signal_group::is_any(uint32_t flags)
{
    std::unique_lock<std::mutex> lock(event_group->m);
    return flags&event_group->flags;
}

bool signal_group::wait_any(uint32_t flags, uint32_t time_ms)
{
    std::unique_lock<std::mutex> lock(event_group->m);
    return event_group->notify.wait_for(lock, std::chrono::milliseconds(time_ms), [&]{ return flags&event_group->flags; });
}

signal_group::~signal_group() = default;

} // namespace esp_modem