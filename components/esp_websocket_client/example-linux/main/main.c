#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_websocket_client.h"

static void tcp_client_task(void *pvParameters)
{
    ESP_LOGI("TAG", "hello world from task");
    vTaskDelete(NULL);
}

int main(int argc , char *argv[])
{

    xTaskCreate(tcp_client_task, "tcp_client", 4096, NULL, 5, NULL);
    ESP_LOGI("TAG", "hello world");
    return 0;
}
