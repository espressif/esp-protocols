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

#ifndef _ESP_MODEM_PRIMITIVES_HPP_
#define _ESP_MODEM_PRIMITIVES_HPP_
#include "esp_modem_exception.hpp"

#if defined(CONFIG_IDF_TARGET_LINUX)
#include <mutex>
#else
// forward declarations of FreeRTOS primitives
struct QueueDefinition;
typedef void * EventGroupHandle_t;
#endif


namespace esp_modem {

#if !defined(CONFIG_IDF_TARGET_LINUX)
struct Lock {
    using MutexT = QueueDefinition*;

    explicit Lock();
    ~Lock();
    void lock();
    void unlock();
private:
    MutexT m{};
};

using Signal = void*;

#else
using Lock = std::mutex;
struct SignalGroup;
using Signal = std::unique_ptr<SignalGroup>;
#endif

template<class T>
class Scoped {
public:
    explicit Scoped(T &l):lock(l) { lock.lock(); }
    ~Scoped() { lock.unlock(); }

private:
    T& lock;
};

struct signal_group {
    static constexpr size_t bit0 = 1 << 0;
    static constexpr size_t bit1 = 1 << 1;
    static constexpr size_t bit2 = 1 << 2;
    static constexpr size_t bit3 = 1 << 3;

    explicit signal_group();

    void set(uint32_t bits);

    void clear(uint32_t bits);

    // waiting for all and clearing if set
    bool wait(uint32_t flags, uint32_t time_ms);

    bool is_any(uint32_t flags);

    // waiting for any bit, not clearing them
    bool wait_any(uint32_t flags, uint32_t time_ms);

    ~signal_group();

private:
    Signal event_group;
};

} // namespace esp_modem

#endif // _ESP_MODEM_PRIMITIVES_HPP_
