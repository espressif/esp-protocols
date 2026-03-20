/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include "media_lib_os.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "media_lib_adapter.h"
#include "media_lib_os.h"
#include "esp_log.h"
#include "esp_webrtc_defaults.h"
#include "esp_peer_default.h"
#include "common.h"
#include "esp_check.h"
#include "peer_impl.h"
#include "sdkconfig.h"

#define WEBRTC_URL (CONFIG_EXAMPLE_WEBRTC_URL CONFIG_EXAMPLE_WEBRTC_ROOM_ID)
#define PEER_CONNECTED      BIT(0)
#define PEER_DISCONNECTED   BIT(1)
#define MAX_BUFFER_SIZE     (4*1024)

static EventGroupHandle_t s_state = NULL;
static const char *TAG = "serverless_mqtt_webrtc";

void peer_get_buffer(char ** buffer, size_t *buffer_len)
{
    static char s_buffer[MAX_BUFFER_SIZE];
    if (buffer && buffer_len) {
        *buffer = s_buffer;
        *buffer_len = MAX_BUFFER_SIZE;
    }
}

static int start_webrtc(char *url);
static int stop_webrtc(void);

static on_peer_recv_t s_on_recv = NULL;
static esp_peer_signaling_handle_t signaling = NULL;
static esp_peer_handle_t peer = NULL;
static bool peer_running = false;

static void thread_scheduler(const char *thread_name, media_lib_thread_cfg_t *thread_cfg)
{
    if (strcmp(thread_name, "pc_task") == 0) {
        thread_cfg->stack_size = 25 * 1024;
        thread_cfg->priority = 18;
        thread_cfg->core_id = 1;
    }
}

esp_err_t peer_init(on_peer_recv_t cb)
{
    esp_err_t ret = ESP_OK;
    s_on_recv = cb;
    s_state = xEventGroupCreate();
    media_lib_add_default_adapter();
    media_lib_thread_set_schedule_cb(thread_scheduler);
    ESP_RETURN_ON_FALSE(s_state, ESP_ERR_NO_MEM, TAG, "Failed to create state event group");
    ESP_GOTO_ON_FALSE(start_webrtc(WEBRTC_URL) == ESP_PEER_ERR_NONE, ESP_FAIL, err, TAG, "Failed to start webRTC");
    ESP_LOGI(TAG, "Waiting for peer to connect");
    int i = 0;
    while (1) {
        EventBits_t bits = xEventGroupWaitBits(s_state, PEER_CONNECTED, pdFALSE, pdFALSE, pdMS_TO_TICKS(1000));
        if (bits & PEER_CONNECTED) {
            ESP_LOGI(TAG, "Peer is connected!");
            return ret;
        }
        ESP_GOTO_ON_FALSE(i++ < 100, ESP_ERR_TIMEOUT, err, TAG, "Peer connection timeout");
        if (peer) {
            esp_peer_query(peer);
        }
    }

err:
    vEventGroupDelete(s_state);
    return ret;
}


static int peer_state_handler(esp_peer_state_t state, void* ctx)
{
    ESP_LOGI(TAG, "Peer state: %d", state);
    if (state == ESP_PEER_STATE_CONNECTED) {
        xEventGroupSetBits(s_state, PEER_CONNECTED);
    } else if (state == ESP_PEER_STATE_DISCONNECTED) {
        xEventGroupSetBits(s_state, PEER_DISCONNECTED);
    }
    return 0;
}

static int peer_msg_handler(esp_peer_msg_t* msg, void* ctx)
{
    if (msg->type == ESP_PEER_MSG_TYPE_SDP) {
        // Send local SDP to signaling server
        esp_peer_signaling_send_msg(signaling, (esp_peer_signaling_msg_t *)msg);
    }
    return 0;
}

static int peer_video_info_handler(esp_peer_video_stream_info_t* info, void* ctx)
{
    return 0;
}

static int peer_audio_info_handler(esp_peer_audio_stream_info_t* info, void* ctx)
{
    return 0;
}

static int peer_audio_data_handler(esp_peer_audio_frame_t* frame, void* ctx)
{
    ESP_LOGI(TAG, "Audio Sequence %d(%d)", (int)frame->pts, (int)frame->data[0]);
    return 0;
}

static int peer_video_data_handler(esp_peer_video_frame_t* frame, void* ctx)
{
    return 0;
}

static int peer_data_handler(esp_peer_data_frame_t* frame, void* ctx)
{
    if (frame && frame->size > 0) {
        s_on_recv((char*)frame->data, frame->size);
    }
    return 0;
}

static void pc_task(void *arg)
{
    while (peer_running) {
        esp_peer_main_loop(peer);
        media_lib_thread_sleep(20);
    }
    media_lib_thread_destroy(NULL);
}

static int signaling_ice_info_handler(esp_peer_signaling_ice_info_t* info, void* ctx)
{
    if (peer == NULL) {
        esp_peer_default_cfg_t peer_cfg = {
            .agent_recv_timeout = 500,
        };
        esp_peer_cfg_t cfg = {
            .server_lists = &info->server_info,
            .server_num = 1,
            .audio_dir = ESP_PEER_MEDIA_DIR_SEND_RECV,
            .audio_info = {
                .codec = ESP_PEER_AUDIO_CODEC_G711A,
            },
            .enable_data_channel = true,
            .role = info->is_initiator ? ESP_PEER_ROLE_CONTROLLING : ESP_PEER_ROLE_CONTROLLED,
            .on_state = peer_state_handler,
            .on_msg = peer_msg_handler,
            .on_video_info = peer_video_info_handler,
            .on_audio_info = peer_audio_info_handler,
            .on_video_data = peer_video_data_handler,
            .on_audio_data = peer_audio_data_handler,
            .on_data = peer_data_handler,
            .ctx = ctx,
            .extra_cfg = &peer_cfg,
            .extra_size = sizeof(esp_peer_default_cfg_t),
        };
        int ret = esp_peer_open(&cfg, esp_peer_get_default_impl(), &peer);
        if (ret != ESP_PEER_ERR_NONE) {
            return ret;
        }
        media_lib_thread_handle_t thread = NULL;
        peer_running = true;
        media_lib_thread_create_from_scheduler(&thread, "pc_task", pc_task, NULL);
        if (thread == NULL) {
            peer_running = false;
        }
    }
    return 0;
}

static int signaling_connected_handler(void* ctx)
{
    if (peer) {
        return esp_peer_new_connection(peer);
    }
    return 0;
}

static int signaling_msg_handler(esp_peer_signaling_msg_t* msg, void* ctx)
{
    if (msg->type == ESP_PEER_SIGNALING_MSG_BYE) {
        esp_peer_close(peer);
        peer = NULL;
    } else if (msg->type == ESP_PEER_SIGNALING_MSG_SDP) {
        // Receive remote SDP
        if (peer) {
            esp_peer_send_msg(peer, (esp_peer_msg_t*)msg);
        }
    }
    return 0;
}

static int signaling_close_handler(void *ctx)
{
    return 0;
}

static int start_signaling(char* url)
{
    esp_peer_signaling_cfg_t cfg = {
        .signal_url = url,
        .on_ice_info = signaling_ice_info_handler,
        .on_connected = signaling_connected_handler,
        .on_msg = signaling_msg_handler,
        .on_close = signaling_close_handler,
    };
    // Use APPRTC signaling
    return esp_peer_signaling_start(&cfg, esp_signaling_get_apprtc_impl(), &signaling);
}

static int start_webrtc(char *url)
{
    stop_webrtc();
    return start_signaling(url);
}

static int stop_webrtc(void)
{
    peer_running = false;
    if (peer) {
        esp_peer_close(peer);
        peer = NULL;
    }
    if (signaling) {
        esp_peer_signaling_stop(signaling);
        signaling = NULL;
    }
    return 0;
}

void peer_send(char* data, size_t size)
{
    esp_peer_data_frame_t data_frame = {
        .type = ESP_PEER_DATA_CHANNEL_DATA,
        .data = (uint8_t*)data,
        .size = size,
    };
    esp_peer_send_data(peer, &data_frame);
}
