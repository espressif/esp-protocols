/*
 * SPDX-FileCopyrightText: 2021-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <pthread.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "osal/osal_api.h"
#include <semaphore.h>

typedef struct task_notifiers {
    sem_t sem;
    TaskHandle_t id;
} task_notifiers_t;

typedef struct pthread_params {
    void *const param;
    TaskFunction_t task;
    bool started;
    TaskHandle_t handle;
} pthread_params_t;

static uint64_t s_semaphore_data = 0;
static task_notifiers_t *s_notifiers;
static int s_threads = 0;
pthread_mutex_t s_mutex;

typedef enum queue_type_tag {
    MUTEX_REC,
    MUTEX,
    SEMA,
    QUEUE,
} queue_type_t;

struct generic_queue_handle {
    queue_type_t type;
    size_t item_size;
    void *q;
};

static struct generic_queue_handle *create_generic_queue(queue_type_t type, uint32_t len, uint32_t item_size)
{
    struct generic_queue_handle *h = calloc(1, sizeof(struct generic_queue_handle));
    h->item_size = len;
    h->type = type;
    switch (type) {
    default:
    case QUEUE:
    case SEMA:
        h->q = osal_queue_create();
        break;

    case MUTEX:
    case MUTEX_REC:
        h->q = osal_mutex_create();
        break;
    }
    return h;
}

QueueHandle_t xQueueCreate(uint32_t uxQueueLength, uint32_t uxItemSize)
{
    return (QueueHandle_t)create_generic_queue(QUEUE, uxQueueLength, uxItemSize);
}

uint32_t xQueueSend(QueueHandle_t xQueue, const void *pvItemToQueue, TickType_t xTicksToWait)
{
    struct generic_queue_handle *h = xQueue;
    return osal_queue_send(h->q, (uint8_t *)pvItemToQueue, h->item_size) ? pdTRUE : pdFAIL;
}

uint32_t xQueueSendToBack(QueueHandle_t xQueue, const void *pvItemToQueue, TickType_t xTicksToWait)
{
    return xQueueSend(xQueue, pvItemToQueue, xTicksToWait);
}

uint32_t xQueueReceive(QueueHandle_t xQueue, void *pvBuffer, TickType_t xTicksToWait)
{
    struct generic_queue_handle *h = xQueue;
    return osal_queue_recv(h->q, (uint8_t *)pvBuffer, h->item_size, xTicksToWait) ? pdTRUE : pdFAIL;
}

BaseType_t xSemaphoreGive(QueueHandle_t xQueue)
{
    struct generic_queue_handle *h = xQueue;
    if (h->type == MUTEX) {
        osal_mutex_give(h->q);
        return pdTRUE;
    }
    return xQueueSend(xQueue, &s_semaphore_data, portMAX_DELAY);
}

BaseType_t xSemaphoreGiveRecursive(QueueHandle_t xQueue)
{
    struct generic_queue_handle *h = xQueue;
    if (h->type == MUTEX_REC) {
        osal_mutex_give(h->q);
        return pdTRUE;
    }
    return pdFALSE;
}

BaseType_t xSemaphoreTake(QueueHandle_t xQueue, TickType_t pvTask)
{
    struct generic_queue_handle *h = xQueue;
    if (h->type == MUTEX) {
        osal_mutex_take(h->q);
        return pdTRUE;
    }
    return xQueueReceive(xQueue, &s_semaphore_data, portMAX_DELAY);
}

BaseType_t xSemaphoreTakeRecursive(QueueHandle_t xQueue, TickType_t pvTask)
{
    struct generic_queue_handle *h = xQueue;
    if (h->type == MUTEX_REC) {
        osal_mutex_take(h->q);
        return pdTRUE;
    }
    return pdFALSE;
}

void vQueueDelete(QueueHandle_t xQueue)
{
    struct generic_queue_handle *h = xQueue;
    if (h->q) {
        if (h->type == MUTEX || h->type == MUTEX_REC) {
            osal_mutex_delete(h->q);
        } else {
            osal_queue_delete(h->q);
        }
    }
    free(xQueue);
}

QueueHandle_t xSemaphoreCreateBinary(void)
{
    return xQueueCreate(1, 1);
}

QueueHandle_t xSemaphoreCreateMutex(void)
{
    return (QueueHandle_t)create_generic_queue(MUTEX, 1, 1);
}

QueueHandle_t xSemaphoreCreateRecursiveMutex(void)
{
    return (QueueHandle_t)create_generic_queue(MUTEX_REC, 1, 1);
}


void vTaskDelete(TaskHandle_t *task)
{
    for (int i = 0; i < s_threads; ++i) {
        if (task == s_notifiers[i].id) {
            sem_destroy(&s_notifiers[i].sem);
            s_notifiers[i].id = 0;
        }
    }

    if (task == NULL) {
        pthread_exit(0);
    }
    void *thread_rval = NULL;
    pthread_join((pthread_t)task, &thread_rval);
}

void vTaskSuspend(void *task)
{
    vTaskDelete(task);
}

TickType_t xTaskGetTickCount(void)
{
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
    return spec.tv_nsec / 1000000 + spec.tv_sec * 1000;
}

void vTaskDelay(const TickType_t xTicksToDelay)
{
    usleep(xTicksToDelay * 1000);
}

void *pthread_task(void *params)
{
    pthread_params_t *pthread_params = params;

    void *const param = pthread_params->param;
    TaskFunction_t task = pthread_params->task;

    pthread_params->handle = xTaskGetCurrentTaskHandle();
    if (s_threads == 0) {
        pthread_mutex_init(&s_mutex, NULL);
    }
    pthread_mutex_lock(&s_mutex);
    s_notifiers = realloc(s_notifiers, sizeof(struct task_notifiers) * (++s_threads));
    assert(s_notifiers);
    s_notifiers[s_threads - 1].id = pthread_params->handle;
    sem_init(&s_notifiers[s_threads - 1].sem, 0, 0);
    pthread_mutex_unlock(&s_mutex);
    pthread_params->started = true;

    task(param);

    return NULL;
}

TaskHandle_t xTaskCreateStaticPinnedToCore(TaskFunction_t pxTaskCode,
                                           const char *const pcName,
                                           const uint32_t ulStackDepth,
                                           void *const pvParameters,
                                           UBaseType_t uxPriority,
                                           StackType_t *const puxStackBuffer,
                                           StaticTask_t *const pxTaskBuffer,
                                           const BaseType_t xCoreID)
{
    static TaskHandle_t pvCreatedTask;
    xTaskCreate(pxTaskCode, pcName, ulStackDepth, pvParameters, uxPriority, &pvCreatedTask);
    return pvCreatedTask;
}

BaseType_t xTaskCreatePinnedToCore(TaskFunction_t pvTaskCode,
                                   const char *const pcName,
                                   const uint32_t usStackDepth,
                                   void *const pvParameters,
                                   UBaseType_t uxPriority,
                                   TaskHandle_t *const pvCreatedTask,
                                   const BaseType_t xCoreID)
{
    xTaskCreate(pvTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pvCreatedTask);
    return pdTRUE;
}

BaseType_t xTaskCreate(TaskFunction_t pvTaskCode, const char *const pcName, const uint32_t usStackDepth, void *const pvParameters, UBaseType_t uxPriority, TaskHandle_t *const pvCreatedTask)
{
    pthread_t new_thread = (pthread_t)NULL;
    pthread_attr_t attr;
    pthread_params_t pthread_params = { .param = pvParameters, .task = pvTaskCode};

    int res = pthread_attr_init(&attr);
    assert(res == 0);
    res = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    assert(res == 0);
    res = pthread_create(&new_thread, &attr, pthread_task, &pthread_params);
    assert(res == 0);

    // just wait till the task started so we can unwind params from the stack
    while (pthread_params.started == false) {
        usleep(1000);
    }
    if (pvCreatedTask) {
        *pvCreatedTask = pthread_params.handle;
    }

    return pdTRUE;
}

void xTaskNotifyGive(TaskHandle_t task)
{
    int i = 0;
    while (true) {
        pthread_mutex_lock(&s_mutex);
        if (task == s_notifiers[i].id) {
            sem_post(&s_notifiers[i].sem);
            pthread_mutex_unlock(&s_mutex);
            return;
        }
        pthread_mutex_unlock(&s_mutex);
        if (++i == s_threads) {
            i = 0;
        }
        usleep(1000);
    }
}

BaseType_t xTaskNotifyWait(uint32_t bits_entry_clear, uint32_t bits_exit_clear, uint32_t *value, TickType_t wait_time)
{
    return true;
}

TaskHandle_t xTaskGetCurrentTaskHandle(void)
{
    return (TaskHandle_t)pthread_self();
}

EventGroupHandle_t xEventGroupCreate(void)
{
    return osal_signal_create();
}

void vEventGroupDelete(EventGroupHandle_t xEventGroup)
{
    osal_signal_delete(xEventGroup);
}

EventBits_t xEventGroupClearBits(EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToClear)
{
    return osal_signal_clear(xEventGroup, uxBitsToClear);
}

EventBits_t xEventGroupGetBits(EventGroupHandle_t xEventGroup)
{
    return osal_signal_get(xEventGroup);
}

EventBits_t xEventGroupSetBits(EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet)
{
    return osal_signal_set(xEventGroup, uxBitsToSet);
}

EventBits_t xEventGroupWaitBits(EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToWaitFor, const BaseType_t xClearOnExit, const BaseType_t xWaitForAllBits, TickType_t xTicksToWait)
{
    return osal_signal_wait(xEventGroup, uxBitsToWaitFor, xWaitForAllBits, xTicksToWait);
}

void ulTaskNotifyTake(bool clear_on_exit, uint32_t xTicksToWait)
{
    TaskHandle_t task = xTaskGetCurrentTaskHandle();
    int i = 0;
    while (true) {
        pthread_mutex_lock(&s_mutex);
        if (task == s_notifiers[i].id) {
            pthread_mutex_unlock(&s_mutex);
            sem_wait(&s_notifiers[i].sem);
            return;
        }
        pthread_mutex_unlock(&s_mutex);
        if (++i == s_threads) {
            i = 0;
        }
        usleep(1000);
    }
}
