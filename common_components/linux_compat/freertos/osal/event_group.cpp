/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <mutex>
#include <condition_variable>
#include "osal_api.h"


class SignalGroup {

    struct SignalGroupInternal {
        std::condition_variable notify;
        std::mutex m;
        uint32_t flags{ 0 };
    };

    using SignalT = std::unique_ptr<SignalGroupInternal>;

public:

    void set(uint32_t bits)
    {
        std::unique_lock<std::mutex> lock(event_group->m);
        event_group->flags |= bits;
        event_group->notify.notify_all();
    }

    uint32_t get()
    {
        return event_group->flags;
    }

    void clear(uint32_t bits)
    {
        std::unique_lock<std::mutex> lock(event_group->m);
        event_group->flags &= ~bits;
        event_group->notify.notify_all();
    }

    // waiting for all and clearing if set
    bool wait(uint32_t flags, uint32_t time_ms)
    {
        std::unique_lock<std::mutex> lock(event_group->m);
        return event_group->notify.wait_for(lock, std::chrono::milliseconds(time_ms), [&] {
            if ((flags & event_group->flags) == flags)
            {
                event_group->flags &= ~flags;
                return true;
            }
            return false;
        });
    }


    // waiting for any bit, not clearing them
    bool wait_any(uint32_t flags, uint32_t time_ms)
    {
        std::unique_lock<std::mutex> lock(event_group->m);
        return event_group->notify.wait_for(lock, std::chrono::milliseconds(time_ms), [&] { return flags & event_group->flags; });
    }

private:
    SignalT event_group{std::make_unique<SignalGroupInternal>()};
};


void *osal_signal_create()
{
    auto signal = new SignalGroup;
    return signal;
}

void osal_signal_delete(void *s)
{
    delete static_cast<SignalGroup *>(s);
}

uint32_t osal_signal_clear(void *s, uint32_t bits)
{
    auto signal = static_cast<SignalGroup *>(s);
    signal->clear(bits);
    return signal->get();
}

uint32_t osal_signal_set(void *s, uint32_t bits)
{
    auto signal = static_cast<SignalGroup *>(s);
    signal->set(bits);
    return signal->get();
}

uint32_t osal_signal_get(void *s)
{
    auto signal = static_cast<SignalGroup *>(s);
    return signal->get();
}

uint32_t osal_signal_wait(void *s, uint32_t flags, bool all, uint32_t timeout)
{
    auto signal = static_cast<SignalGroup *>(s);
    if (all) {
        signal->wait(flags, timeout);
    } else {
        signal->wait_any(flags, timeout);
    }
    return signal->get();
}
