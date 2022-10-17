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

#include <cstring>
#include <unistd.h>
#include <cxx_include/esp_modem_cmux.hpp>
#include "cxx_include/esp_modem_dte.hpp"
#include "esp_log.h"

using namespace esp_modem;

/**
 * @brief Define this to defragment partially received data of CMUX payload
 *        This is useful if upper layers expect the entire payload available
 *        for parsing.
 */
#define DEFRAGMENT_CMUX_PAYLOAD

#define EA 0x01  /* Extension bit      */
#define CR 0x02  /* Command / Response */
#define PF 0x10  /* Poll / Final       */

/* Frame types */
#define FT_RR      0x01  /* Receive Ready                            */
#define FT_UI      0x03  /* Unnumbered Information                   */
#define FT_RNR     0x05  /* Receive Not Ready                        */
#define FT_REJ     0x09  /* Reject                                   */
#define FT_DM      0x0F  /* Disconnected Mode                        */
#define FT_SABM    0x2F  /* Set Asynchronous Balanced Mode           */
#define FT_DISC    0x43  /* Disconnect                               */
#define FT_UA      0x63  /* Unnumbered Acknowledgement               */
#define FT_UIH     0xEF  /* Unnumbered Information with Header check */

/* Control channel commands */
#define CMD_NSC    0x08  /* Non Supported Command Response           */
#define CMD_TEST   0x10  /* Test Command                             */
#define CMD_PSC    0x20  /* Power Saving Control                     */
#define CMD_RLS    0x28  /* Remote Line Status Command               */
#define CMD_FCOFF  0x30  /* Flow Control Off Command                 */
#define CMD_PN     0x40  /* DLC parameter negotiation                */
#define CMD_RPN    0x48  /* Remote Port Negotiation Command          */
#define CMD_FCON   0x50  /* Flow Control On Command                  */
#define CMD_CLD    0x60  /* Multiplexer close down                   */
#define CMD_SNC    0x68  /* Service Negotiation Command              */
#define CMD_MSC    0x70  /* Modem Status Command                     */

/* Flag sequence field between messages (start of frame) */
#define SOF_MARKER 0xF9

uint8_t CMux::fcs_crc(const uint8_t frame[6])
{
    //    #define FCS_GOOD_VALUE 0xCF
    uint8_t crc = 0xFF; // FCS_INIT_VALUE

    for (int i = 1; i < 4; i++) {
        crc ^= frame[i];

        for (int j = 0; j < 8; j++) {
            if (crc & 0x01) {
                crc = (crc >> 1) ^ 0xe0; // FCS_POLYNOMIAL
            } else {
                crc >>= 1;
            }
        }
    }

    return crc;
}

void CMux::send_sabm(size_t i)
{
    uint8_t frame[6];
    frame[0] = SOF_MARKER;
    frame[1] = (i << 2) | 0x3;
    frame[2] = FT_SABM | PF;
    frame[3] = 1;
    frame[4] = 0xFF - fcs_crc(frame);
    frame[5] = SOF_MARKER;
    term->write(frame, 6);
}


void CMux::data_available(uint8_t *data, size_t len)
{
    if (data && type == 0xFF && len > 0 && dlci > 0) {
        int virtual_term = dlci - 1;
        if (virtual_term < max_terms && read_cb[virtual_term]) {
            // Post partial data (or defragment to post on CMUX footer)
#ifdef DEFRAGMENT_CMUX_PAYLOAD
            if (payload_start == nullptr) {
                payload_start = data;
                total_payload_size = 0;
            }
            total_payload_size += len;
#else
            read_cb[virtual_term](data, len);
#endif
        }
    } else if (data == nullptr && type == 0x73 && len == 0) { // notify the initial SABM command
        Scoped<Lock> l(lock);
        sabm_ack = dlci;
    } else if (data == nullptr) {
        int virtual_term = dlci - 1;
        if (virtual_term < max_terms && read_cb[virtual_term]) {
#ifdef DEFRAGMENT_CMUX_PAYLOAD
            read_cb[virtual_term](payload_start, total_payload_size);
#endif
        }

    }
}

bool CMux::on_cmux(uint8_t *data, size_t actual_len)
{
    if (!data) {
#ifdef DEFRAGMENT_CMUX_PAYLOAD
        auto data_to_read = buffer_size - 128; // keep 128 (max CMUX payload) backup buffer)
        if (payload_start) {
            data = payload_start + total_payload_size;
            data_to_read = payload_len + 2;
        } else {
            data = buffer.get();
        }
        actual_len = term->read(data, data_to_read);
#else
        data = buffer.get();
        actual_len = term->read(data, buffer_size);
#endif
    }
    ESP_LOG_BUFFER_HEXDUMP("CMUX Received", data, actual_len, ESP_LOG_DEBUG);
    uint8_t* frame = data;
    uint8_t* recover_ptr;
    auto available_len = actual_len;
    size_t payload_offset = 0;
    size_t footer_offset = 0;
    while (available_len > 0) {
        switch (state) {
            case cmux_state::RECOVER:
                if (frame[0] == SOF_MARKER) {
                    // already init state
                    state = cmux_state::INIT;
                    break;
                }
                recover_ptr = static_cast<uint8_t*>(memchr(frame, SOF_MARKER, available_len));
                if (recover_ptr && available_len > recover_ptr - frame) {
                    available_len -= (recover_ptr - frame);
                    frame = recover_ptr;
                    state = cmux_state::INIT;
                    ESP_LOGI("CMUX", "Protocol recovered");
                    if (available_len > 1 && frame[1] == SOF_MARKER) {
                        // empty frame
                        available_len -= 1;
                        frame += 1;
                    }
                    break;
                }
                // marker not found, continue with recovery
                return false;
            case cmux_state::INIT:
                if (frame[0] != SOF_MARKER) {
                    ESP_LOGW("CMUX", "Protocol mismatch: Missed leading SOF, recovering...");
                    state = cmux_state::RECOVER;
                    break;
                }
                if (available_len > 1 && frame[1] == SOF_MARKER) {
                    // empty frame
                    available_len -= 1;
                    frame += 1;
                    break;
                }
                state = cmux_state::HEADER;
                available_len--;
                frame_header_offset = 1;
                frame++;
                break;
            case cmux_state::HEADER:
                if (available_len > 0 && frame_header_offset == 1 && frame[0] == SOF_MARKER) {
                    // Previously trailing SOF interpreted as heading SOF, remove it and restart HEADER
                    available_len--;
                    frame++;
                    break;
                }
                if (available_len + frame_header_offset < 4) {
                    memcpy(frame_header + frame_header_offset, frame, available_len);
                    frame_header_offset += available_len;
                    return false; // need read more
                }
                payload_offset = std::min(available_len, 4 - frame_header_offset);
                memcpy(frame_header + frame_header_offset, frame, payload_offset);
                frame_header_offset += payload_offset;
                dlci = frame_header[1] >> 2;
                type = frame_header[2];
                payload_len = (frame_header[3] >> 1);
                frame += payload_offset;
                available_len -= payload_offset;
                state = cmux_state::PAYLOAD;
                break;
            case cmux_state::PAYLOAD:
                ESP_LOGD("CMUX", "Payload frame: dlci:%02x type:%02x payload:%d available:%d", dlci, type, payload_len, available_len);
                if (available_len < payload_len) { // payload
                    state = cmux_state::PAYLOAD;
                    data_available(frame, available_len); // partial read
                    payload_len -= available_len;
                    return false;
                } else { // complete
                    if (payload_len > 0) {
                        data_available(&frame[0], payload_len); // rest read
                    }
                    available_len -= payload_len;
                    frame += payload_len;
                    state = cmux_state::FOOTER;
                    payload_len = 0;
                }
                break;
            case cmux_state::FOOTER:
                if (available_len + frame_header_offset < 6) {
                    memcpy(frame_header + frame_header_offset, frame, available_len);
                    frame_header_offset += available_len;
                    return false; // need read more
                } else {
                    footer_offset = std::min(available_len, 6 - frame_header_offset);
                    memcpy(frame_header + frame_header_offset, frame, footer_offset);
                    if (frame_header[5] != SOF_MARKER) {
                        ESP_LOGW("CMUX", "Protocol mismatch: Missed trailing SOF, recovering...");
                        payload_start = nullptr;
                        total_payload_size = 0;
                        state = cmux_state::RECOVER;
                        break;
                    }
                    frame += footer_offset;
                    available_len -= footer_offset;
                    state = cmux_state::INIT;
                    frame_header_offset = 0;
                    data_available(nullptr, 0);
                    payload_start = nullptr;
                    total_payload_size = 0;
                }
                break;
        }
    }
    return true;
}

bool CMux::init()
{
    frame_header_offset = 0;
    state = cmux_state::INIT;
    term->set_read_cb([this](uint8_t *data, size_t len) {
        this->on_cmux(data, len);
        return false;
    });

    sabm_ack = -1;
    for (size_t i = 0; i < 3; i++)
    {
        int timeout = 0;
        send_sabm(i);
        while (true) {
            usleep(10'000);
            Scoped<Lock> l(lock);
            if (sabm_ack == i) {
                sabm_ack = -1;
                break;
            }
            if (timeout++ > 100)
                return false;
        }
    }
    return true;
}

int CMux::write(int virtual_term, uint8_t *data, size_t len)
{
    const size_t cmux_max_len = 127;
    Scoped<Lock> l(lock);
    int i = virtual_term + 1;
    size_t need_write = len;
    while (need_write > 0) {
        size_t batch_len = need_write;
        if (batch_len > cmux_max_len)
            batch_len = cmux_max_len;
        uint8_t frame[6];
        frame[0] = SOF_MARKER;
        frame[1] = (i << 2) + 1;
        frame[2] = FT_UIH;
        frame[3] = (batch_len << 1) + 1;
        frame[4] = 0xFF - fcs_crc(frame);
        frame[5] = SOF_MARKER;

        term->write(frame, 4);
        term->write(data, batch_len);
        term->write(frame + 4, 2);
        ESP_LOG_BUFFER_HEXDUMP("Send", frame, 4, ESP_LOG_VERBOSE);
        ESP_LOG_BUFFER_HEXDUMP("Send", data, batch_len, ESP_LOG_VERBOSE);
        ESP_LOG_BUFFER_HEXDUMP("Send", frame+4, 2, ESP_LOG_VERBOSE);
        need_write -= batch_len;
        data += batch_len;
    }
    return len;
}

void CMux::set_read_cb(int inst, std::function<bool(uint8_t *, size_t)> f) {
    if (inst < max_terms)
        read_cb[inst] = std::move(f);
}
