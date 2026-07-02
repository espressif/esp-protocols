/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * Stress-test concurrent getaddrinfo() while esp_dns owns the lwIP resolver hook (DoT / DoH).
 * Intended to surface Issue 1 (singleton shared state) and Issue 4 (DoH shared response buffer).
 */
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_crt_bundle.h"
#include "net_connect.h"
#include "esp_dns.h"

#define TAG "esp_dns_concurrent_test"

/** Number of worker tasks started together (tune up to stress the singleton resolver). */
#define NUM_WORKERS 3

/** Lookups each worker performs after the synchronized start. */
#define ITERATIONS_PER_TASK 25

/**
 * ESP-IDF's xTaskCreate() stack depth is in bytes.
 * Secure DNS + getaddrinfo can use a large stack due to TLS/HTTP parsing.
 */
#define DNS_TASK_STACK_BYTES 8192

#define START_BIT BIT0

static const char *s_hostnames[] = {
    "www.cloudflare.com",
    "www.google.com",
    "example.com",
    "one.one.one.one",
};

#define HOSTNAME_COUNT (sizeof(s_hostnames) / sizeof(s_hostnames[0]))

typedef struct {
    bool is_doh;
    int worker_id;
} worker_param_t;

typedef struct {
    bool is_doh;
    bool ok;
    SemaphoreHandle_t done_sem;
} warmup_param_t;

static EventGroupHandle_t s_start_event;
static SemaphoreHandle_t s_join_sem;

static worker_param_t s_worker_params[NUM_WORKERS];

static portMUX_TYPE s_stats_mux = portMUX_INITIALIZER_UNLOCKED;
static uint32_t s_attempts;
static uint32_t s_success;
static uint32_t s_failures;
static bool s_have_first_fail;
static int s_first_fail_status;
static const char *s_first_fail_host;

static void reset_stats(void)
{
    portENTER_CRITICAL(&s_stats_mux);
    s_attempts = 0;
    s_success = 0;
    s_failures = 0;
    s_have_first_fail = false;
    s_first_fail_status = 0;
    s_first_fail_host = NULL;
    portEXIT_CRITICAL(&s_stats_mux);
}

static void record_attempt(void)
{
    portENTER_CRITICAL(&s_stats_mux);
    s_attempts++;
    portEXIT_CRITICAL(&s_stats_mux);
}

static void record_success(void)
{
    portENTER_CRITICAL(&s_stats_mux);
    s_success++;
    portEXIT_CRITICAL(&s_stats_mux);
}

static void record_failure(int status, const char *host)
{
    portENTER_CRITICAL(&s_stats_mux);
    s_failures++;
    if (!s_have_first_fail) {
        s_have_first_fail = true;
        s_first_fail_status = status;
        s_first_fail_host = host;
    }
    portEXIT_CRITICAL(&s_stats_mux);
}

static void dns_worker(void *arg)
{
    worker_param_t *param = (worker_param_t *)arg;
    const char *proto = param->is_doh ? "DoH" : "DoT";
    const int wid = param->worker_id;

    (void)xEventGroupWaitBits(s_start_event, START_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

    for (int n = 0; n < ITERATIONS_PER_TASK; n++) {
        const char *host = s_hostnames[(n + wid) % HOSTNAME_COUNT];
        struct addrinfo hints;
        struct addrinfo *res = NULL;

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;

        record_attempt();
        const int st = getaddrinfo(host, NULL, &hints, &res);
        if (st != 0) {
            record_failure(st, host);
            ESP_LOGW(TAG, "[%s worker %d] getaddrinfo(%s) failed: %d", proto, wid, host, st);
        } else {
            record_success();
            freeaddrinfo(res);
        }
    }

    (void)xSemaphoreGive(s_join_sem);
    vTaskDelete(NULL);
}

static void print_summary(const char *mode_label, bool is_doh_mode)
{
    uint32_t att;
    uint32_t ok;
    uint32_t fail;
    int first_st;
    const char *first_host;

    portENTER_CRITICAL(&s_stats_mux);
    att = s_attempts;
    ok = s_success;
    fail = s_failures;
    first_st = s_first_fail_status;
    first_host = s_first_fail_host;
    portEXIT_CRITICAL(&s_stats_mux);

    ESP_LOGI(TAG, "=== %s summary ===", mode_label);
    ESP_LOGI(TAG, "attempts=%lu success=%lu failures=%lu",
             (unsigned long)att, (unsigned long)ok, (unsigned long)fail);
    if (fail > 0) {
        ESP_LOGW(TAG, "First failure: status=%d host=%s", first_st, first_host ? first_host : "?");
        if (!is_doh_mode) {
            ESP_LOGW(TAG, "Issue 1 reproduced: concurrent DoT getaddrinfo failures observed (singleton shared state).");
        } else {
            ESP_LOGW(TAG, "Issue 4 reproduced: concurrent DoH getaddrinfo failures observed (shared HTTP response buffer).");
            ESP_LOGW(TAG, "Note: DoH also uses singleton handle fields; Issue 1 may contribute.");
        }
    } else {
        ESP_LOGI(TAG, "No failures in this run (try increasing NUM_WORKERS / ITERATIONS_PER_TASK).");
    }
}

static esp_err_t prepare_sync_objects(void)
{
    if (s_start_event == NULL) {
        s_start_event = xEventGroupCreate();
    }
    if (s_join_sem == NULL) {
        s_join_sem = xSemaphoreCreateCounting(NUM_WORKERS, 0);
    }
    if (s_start_event == NULL || s_join_sem == NULL) {
        ESP_LOGE(TAG, "Failed to create sync objects");
        return ESP_ERR_NO_MEM;
    }
    (void)xEventGroupClearBits(s_start_event, START_BIT);
    return ESP_OK;
}

static void teardown_sync_objects(void)
{
    if (s_join_sem != NULL) {
        vSemaphoreDelete(s_join_sem);
        s_join_sem = NULL;
    }
    if (s_start_event != NULL) {
        vEventGroupDelete(s_start_event);
        s_start_event = NULL;
    }
}

/**
 * @return true if we should wait for @p started tasks on s_join_sem
 */
static bool run_concurrent_workers(bool is_doh)
{
    char task_name[12];
    int started = 0;

    if (prepare_sync_objects() != ESP_OK) {
        return false;
    }

    for (int i = 0; i < NUM_WORKERS; i++) {
        snprintf(task_name, sizeof(task_name), "dns_%d", i);
        s_worker_params[i].is_doh = is_doh;
        s_worker_params[i].worker_id = i;
        BaseType_t ok = xTaskCreate(dns_worker, task_name, DNS_TASK_STACK_BYTES,
                                    &s_worker_params[i], tskIDLE_PRIORITY + 5, NULL);
        if (ok != pdPASS) {
            ESP_LOGE(TAG, "xTaskCreate failed for worker %d", i);
            break;
        }
        started++;
    }

    if (started == 0) {
        ESP_LOGE(TAG, "No worker tasks started");
        teardown_sync_objects();
        return false;
    }

    /* Let workers block on the start event before we signal. */
    vTaskDelay(pdMS_TO_TICKS(150));
    (void)xEventGroupSetBits(s_start_event, START_BIT);

    for (int i = 0; i < started; i++) {
        (void)xSemaphoreTake(s_join_sem, portMAX_DELAY);
    }

    teardown_sync_objects();
    return true;
}

/**
 * Ensure the secure resolver path works with a single synchronous lookup before workers start.
 */
static bool dns_warmup_lookup_once(bool is_doh)
{
    const char *proto = is_doh ? "DoH" : "DoT";
    struct addrinfo hints;
    struct addrinfo *res = NULL;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    for (size_t i = 0; i < HOSTNAME_COUNT; i++) {
        const char *host = s_hostnames[i];
        const int st = getaddrinfo(host, NULL, &hints, &res);
        if (st == 0) {
            freeaddrinfo(res);
            ESP_LOGI(TAG, "[%s] Warmup OK: getaddrinfo(%s)", proto, host);
            return true;
        }
        ESP_LOGW(TAG, "[%s] Warmup: getaddrinfo(%s) failed: %d", proto, host, st);
    }
    ESP_LOGE(TAG, "[%s] Warmup failed: no successful DNS query", proto);
    return false;
}

static void dns_warmup_task(void *arg)
{
    warmup_param_t *param = (warmup_param_t *)arg;

    param->ok = dns_warmup_lookup_once(param->is_doh);
    (void)xSemaphoreGive(param->done_sem);
    vTaskDelete(NULL);
}

static bool dns_warmup_one_success(bool is_doh)
{
    const char *proto = is_doh ? "DoH" : "DoT";
    SemaphoreHandle_t done_sem = xSemaphoreCreateBinary();
    if (done_sem == NULL) {
        ESP_LOGE(TAG, "[%s] Warmup failed: no semaphore", proto);
        return false;
    }

    warmup_param_t param = {
        .is_doh = is_doh,
        .ok = false,
        .done_sem = done_sem,
    };

    BaseType_t task_ok = xTaskCreate(dns_warmup_task, "dns_warmup", DNS_TASK_STACK_BYTES,
                                     &param, tskIDLE_PRIORITY + 5, NULL);
    if (task_ok != pdPASS) {
        ESP_LOGE(TAG, "[%s] Warmup failed: xTaskCreate failed", proto);
        vSemaphoreDelete(done_sem);
        return false;
    }

    (void)xSemaphoreTake(done_sem, portMAX_DELAY);
    vSemaphoreDelete(done_sem);
    return param.ok;
}

static esp_err_t run_mode_dot(void)
{
    esp_dns_config_t cfg = {
        .dns_server = "1dot1dot1dot1.cloudflare-dns.com",
        .port = ESP_DNS_DEFAULT_DOT_PORT,
        .timeout_ms = ESP_DNS_DEFAULT_TIMEOUT_MS,
        .tls_config = {
            .crt_bundle_attach = esp_crt_bundle_attach,
        },
    };

    esp_dns_handle_t h = esp_dns_init_dot(&cfg);
    if (h == NULL) {
        ESP_LOGE(TAG, "esp_dns_init_dot failed");
        return ESP_FAIL;
    }

    if (!dns_warmup_one_success(false)) {
        esp_dns_cleanup_dot(h);
        return ESP_FAIL;
    }

    reset_stats();
    ESP_LOGI(TAG, "Starting DoT concurrent stress (%d tasks x %d lookups)...", NUM_WORKERS, ITERATIONS_PER_TASK);

    if (!run_concurrent_workers(false)) {
        esp_dns_cleanup_dot(h);
        return ESP_FAIL;
    }

    print_summary("DoT concurrency", false);

    esp_dns_cleanup_dot(h);
    return ESP_OK;
}

static esp_err_t run_mode_doh(void)
{
    esp_dns_config_t cfg = {
        .dns_server = "1dot1dot1dot1.cloudflare-dns.com",
        .port = ESP_DNS_DEFAULT_DOH_PORT,
        .timeout_ms = ESP_DNS_DEFAULT_TIMEOUT_MS,
        .protocol_config.doh_config.url_path = "dns-query",
        .tls_config = {
            .crt_bundle_attach = esp_crt_bundle_attach,
        },
    };

    esp_dns_handle_t h = esp_dns_init_doh(&cfg);
    if (h == NULL) {
        ESP_LOGE(TAG, "esp_dns_init_doh failed");
        return ESP_FAIL;
    }

    if (!dns_warmup_one_success(true)) {
        esp_dns_cleanup_doh(h);
        return ESP_FAIL;
    }

    reset_stats();
    ESP_LOGI(TAG, "Starting DoH concurrent stress (%d tasks x %d lookups)...", NUM_WORKERS, ITERATIONS_PER_TASK);

    if (!run_concurrent_workers(true)) {
        esp_dns_cleanup_doh(h);
        return ESP_FAIL;
    }

    print_summary("DoH concurrency", true);

    esp_dns_cleanup_doh(h);
    return ESP_OK;
}

void app_main(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(net_connect());

    ESP_LOGI(TAG, "Network ready. Cloudflare DoT/DoH + concurrent getaddrinfo (see ISSUES_AND_PROGRESS.md Issues 1 & 4).");

    if (run_mode_dot() != ESP_OK) {
        ESP_LOGE(TAG, "DoT mode failed to complete");
    }

    vTaskDelay(pdMS_TO_TICKS(500));

    if (run_mode_doh() != ESP_OK) {
        ESP_LOGE(TAG, "DoH mode failed to complete");
    }

    ESP_LOGI(TAG, "Done. Check logs above for failure counts and Issue 1 / Issue 4 lines.");
}
