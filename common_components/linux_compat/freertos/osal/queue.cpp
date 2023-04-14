/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstdint>
#include <vector>
#include <cstring>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include "osal_api.h"

template <class T>
class Queue {
public:
    void send(std::unique_ptr<T> t)
    {
        std::lock_guard<std::mutex> lock(m);
        q.push(std::move(t));
        c.notify_one();
    }

    std::unique_ptr<T> receive(std::chrono::milliseconds ms)
    {
        std::unique_lock<std::mutex> lock(m);
        while (q.empty()) {
            if (c.wait_for(lock, ms) == std::cv_status::timeout) {
                return nullptr;
            }
        }
        std::unique_ptr<T> val = std::move(q.front());
        q.pop();
        return val;
    }

private:
    std::queue<std::unique_ptr<T>> q{};
    mutable std::mutex m{};
    std::condition_variable c{};
};

using item_t = std::vector<uint8_t>;

void *osal_queue_create(void)
{
    auto *q = new Queue<item_t>();
    return q;
}

void osal_queue_delete(void *q)
{
    auto *queue = static_cast<Queue<item_t> *>(q);
    delete (queue);
}

bool osal_queue_send(void *q, uint8_t *data, size_t len)
{
    auto v = std::make_unique<item_t>(len);
    v->assign(data, data + len);
    auto queue = static_cast<Queue<item_t> *>(q);
    queue->send(std::move(v));
    return true;
}


bool osal_queue_recv(void *q, uint8_t *data, size_t len, uint32_t ms)
{
    auto queue = static_cast<Queue<item_t> *>(q);
    auto v = queue->receive(std::chrono::milliseconds(ms));
    if (v != nullptr) {
        memcpy(data, (void *)v->data(), len);
        return true;
    }
    return false;
}
