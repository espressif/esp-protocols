/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
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

static uint64_t s_semaphore_data = 0;

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

QueueHandle_t xQueueCreate(uint32_t uxQueueLength, uint32_t uxItemSize )
{
    return (QueueHandle_t)create_generic_queue(QUEUE, uxQueueLength, uxItemSize);
}

uint32_t xQueueSend(QueueHandle_t xQueue, const void *pvItemToQueue, TickType_t xTicksToWait)
{
    struct generic_queue_handle *h = xQueue;
    return osal_queue_send(h->q, (uint8_t *)pvItemToQueue, h->item_size) ? pdTRUE : pdFAIL;
}

uint32_t xQueueSendToBack(QueueHandle_t xQueue, const void *pvItemToQueue, TickType_t xTicksToWait )
{
    return xQueueSend(xQueue, pvItemToQueue, xTicksToWait);
}

uint32_t xQueueReceive(QueueHandle_t xQueue, void *pvBuffer, TickType_t xTicksToWait)
{
    struct generic_queue_handle *h = xQueue;
    return osal_queue_recv(h->q, (uint8_t *)pvBuffer, h->item_size, xTicksToWait) ? pdTRUE : pdFAIL;
}

BaseType_t xSemaphoreGive( QueueHandle_t xQueue)
{
    struct generic_queue_handle *h = xQueue;
    if (h->type == MUTEX) {
        osal_mutex_give(h->q);
        return pdTRUE;
    }
    return xQueueSend(xQueue, &s_semaphore_data, portMAX_DELAY);
}

BaseType_t xSemaphoreGiveRecursive( QueueHandle_t xQueue)
{
    struct generic_queue_handle *h = xQueue;
    if (h->type == MUTEX_REC) {
        osal_mutex_give(h->q);
        return pdTRUE;
    }
    return pdFALSE;
}
BaseType_t xSemaphoreTake( QueueHandle_t xQueue, TickType_t pvTask )
{
    struct generic_queue_handle *h = xQueue;
    if (h->type == MUTEX) {
        osal_mutex_take(h->q);
        return pdTRUE;
    }
    return xQueueReceive(xQueue, &s_semaphore_data, portMAX_DELAY);
}


BaseType_t xSemaphoreTakeRecursive( QueueHandle_t xQueue, TickType_t pvTask )
{
    struct generic_queue_handle *h = xQueue;
    if (h->type == MUTEX_REC) {
        osal_mutex_take(h->q);
        return pdTRUE;
    }
    return pdFALSE;
}




void vQueueDelete( QueueHandle_t xQueue )
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
    QueueHandle_t sempaphore =  xQueueCreate(1, 1);
    return sempaphore;
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

TickType_t xTaskGetTickCount( void )
{
    struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
    return spec.tv_nsec / 1000000 + spec.tv_sec * 1000;
}

void vTaskDelay( const TickType_t xTicksToDelay )
{
    usleep(xTicksToDelay * 1000);
}

void *pthread_task(void *params)
{
    struct {
        void *const param;
        TaskFunction_t task;
        bool started;
    } *pthread_params = params;

    void *const param = pthread_params->param;
    TaskFunction_t task = pthread_params->task;
    pthread_params->started = true;

    task(param);

    return NULL;
}

BaseType_t xTaskCreatePinnedToCore( TaskFunction_t pvTaskCode,
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
    struct {
        void *const param;
        TaskFunction_t task;
        bool started;
    } pthread_params = { .param = pvParameters, .task = pvTaskCode};
    int res = pthread_attr_init(&attr);
    assert(res == 0);
    res = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    assert(res == 0);
    res = pthread_create(&new_thread, &attr, pthread_task, &pthread_params);
    assert(res == 0);

    if (pvCreatedTask) {
        *pvCreatedTask = (void *)new_thread;
    }

    // just wait till the task started so we can unwind params from the stack
    while (pthread_params.started == false) {
        usleep(1000);
    }
    return pdTRUE;
}

void xTaskNotifyGive(TaskHandle_t task)
{

}

BaseType_t xTaskNotifyWait(uint32_t bits_entry_clear, uint32_t bits_exit_clear, uint32_t *value, TickType_t wait_time )
{
    return true;
}

TaskHandle_t xTaskGetCurrentTaskHandle(void)
{
    return NULL;
}

EventGroupHandle_t xEventGroupCreate( void )
{
    return osal_signal_create();
}

void vEventGroupDelete( EventGroupHandle_t xEventGroup )
{
    osal_signal_delete(xEventGroup);
}

EventBits_t xEventGroupClearBits( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToClear )
{
    return osal_signal_clear(xEventGroup, uxBitsToClear);
}

EventBits_t xEventGroupGetBits( EventGroupHandle_t xEventGroup)
{
    return osal_signal_get(xEventGroup);
}

EventBits_t xEventGroupSetBits( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToSet )
{
    return osal_signal_set(xEventGroup, uxBitsToSet);
}

EventBits_t xEventGroupWaitBits( EventGroupHandle_t xEventGroup, const EventBits_t uxBitsToWaitFor, const BaseType_t xClearOnExit, const BaseType_t xWaitForAllBits, TickType_t xTicksToWait )
{
    return osal_signal_wait(xEventGroup, uxBitsToWaitFor, xWaitForAllBits, xTicksToWait);
}
