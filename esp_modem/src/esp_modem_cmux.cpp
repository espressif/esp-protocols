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
#include "cxx_include/esp_modem_dte.hpp"
#include "esp_log.h"

using namespace esp_modem;

/* CRC8 is the reflected CRC8/ROHC algorithm */
#define FCS_POLYNOMIAL 0xe0 /* reversed crc8 */
#define FCS_INIT_VALUE 0xFF
#define FCS_GOOD_VALUE 0xCF

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
static uint8_t crc8(const uint8_t *src, size_t len, uint8_t polynomial, uint8_t initial_value,
             bool reversed)
{
    uint8_t crc = initial_value;
    size_t i, j;

    for (i = 0; i < len; i++) {
        crc ^= src[i];

        for (j = 0; j < 8; j++) {
            if (reversed) {
                if (crc & 0x01) {
                    crc = (crc >> 1) ^ polynomial;
                } else {
                    crc >>= 1;
                }
            } else {
                if (crc & 0x80) {
                    crc = (crc << 1) ^ polynomial;
                } else {
                    crc <<= 1;
                }
            }
        }
    }

    return crc;
}

void CMUXedTerminal::start()
{
    for (size_t i = 0; i < 3; i++)
    {
        send_sabm(i);
        vTaskDelay(100 / portTICK_PERIOD_MS); // Waiting before open next DLC
    }
}

void CMUXedTerminal::send_sabm(size_t dlci)
{
    uint8_t frame[6];
    frame[0] = SOF_MARKER;
    frame[1] = (dlci << 2) | 0x3;
    frame[2] = FT_SABM | PF;
    frame[3] = 1;
    frame[4] = 0xFF - crc8(&frame[1], 3, FCS_POLYNOMIAL, FCS_INIT_VALUE, true);
    frame[5] = SOF_MARKER;
    term->write(frame, 6);
}

bool CMUXedTerminal::process_cmux_recv(size_t len)
{
    return false;
}

bool output(uint8_t *data, size_t len, std::string message)
{
    printf("OUTPUT: %s len=%d \n", message.c_str(), len);
    for (int i=0; i< len; ++i) {
        printf("0x%02x, ",data[i]);
    }
    printf("----\n");

    printf("%.*s", len, data);
    return true;
}

bool CMUXedTerminal::on_cmux(size_t len)
{
    auto data_to_read = std::min(len, buffer_size);
    auto data = buffer.get();
    auto actual_len = term->read(data, data_to_read);
//    consumed += actual_len;
    ESP_LOG_BUFFER_HEXDUMP("Received", data, actual_len, ESP_LOG_INFO);
    for (int i=0; i< len; ++i) {
        printf("0x%02x, ",data[i]);
    }
    printf("\n");
    uint8_t* frame = data;
    auto available_len = len;
    size_t payload_offset = 0;
    size_t footer_offset = 0;
    while (available_len > 0) {
        switch (state) {
            case cmux_state::RECOVER:
                // TODO: Implement recovery, looking for SOF_MARKER's
                break;
            case cmux_state::INIT:
                if (frame[0] != SOF_MARKER) {
                    ESP_LOGW("CMUX", "TODO: Recover!");
                    return true;
                }
                state = cmux_state::HEADER;
                available_len--;
                frame_header_offset = 1;
                frame++;
                break;
            case cmux_state::HEADER:
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
                ESP_LOGW("CMUX", "CMUX FR: A:%02x T:%02x L:%d consumed:%d", dlci, type, payload_len, 0);
                frame += payload_offset;
                available_len -= payload_offset;
                state = cmux_state::PAYLOAD;
                break;
            case cmux_state::PAYLOAD:
                if (available_len < payload_len) { // payload
                    state = cmux_state::PAYLOAD;
                    output(frame, available_len, "PAYLOAD partial read"); // partial read
                    payload_len -= available_len;
                    return false;
                } else { // complete
                    if (payload_len > 0) {
                        output(&frame[0], payload_len, "PAYLOAD full read"); // rest read
                    }
                    available_len -= payload_len;
                    frame += payload_len;
                    state = cmux_state::FOOTER;
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
                        ESP_LOGW("CMUX", "TODO: Recover!");
                        return true;
                    }
                    if (payload_len == 0) {
                        output(frame_header, 0, "Null payload");
                    }
                    frame += footer_offset;
                    available_len -= footer_offset;
                    state = cmux_state::INIT;
                    frame_header_offset = 0;
                }
                break;
        }
    }
    return true;
}
void CMUXedTerminal::setup_cmux()
{
    frame_header_offset = 0;
    state = cmux_state::INIT;
    term->set_data_cb([this](size_t len){
        this->on_cmux(len);
        return false;
    });

    for (size_t i = 0; i < 3; i++)
    {
        send_sabm(i);
        vTaskDelay(100 / portTICK_PERIOD_MS); // Waiting before open next DLC
    }
}

void DTE::setup_cmux()
{
    auto original_term = std::move(term);
    auto cmux_term = std::make_unique<CMUXedTerminal>(std::move(original_term), std::move(buffer), buffer_size);
    buffer_size = 0;
    cmux_term->setup_cmux();
    term = std::move(cmux_term);
}


void DTE::send_cmux_command(uint8_t i, const std::string& command)
{
//    send_sabm(i);
//    vTaskDelay(100 / portTICK_PERIOD_MS); // Waiting before open next DLC

    uint8_t frame[6];
    frame[0] = SOF_MARKER;
    frame[1] = (i << 2) + 1;
    frame[2] = FT_UIH;
    frame[3] = (command.length() << 1) + 1;
    frame[4] = 0xFF - crc8(&frame[1], 3, FCS_POLYNOMIAL, FCS_INIT_VALUE, true);
    frame[5] = SOF_MARKER;

    term->write(frame, 4);
    term->write((uint8_t *)command.c_str(), command.length());
    term->write(frame + 4, 2);
    ESP_LOG_BUFFER_HEXDUMP("Send", frame, 4, ESP_LOG_INFO);
    ESP_LOG_BUFFER_HEXDUMP("Send", (uint8_t *)command.c_str(), command.length(), ESP_LOG_INFO);
    ESP_LOG_BUFFER_HEXDUMP("Send", frame+4, 2, ESP_LOG_INFO);
}

int CMUXedTerminal::write(uint8_t *data, size_t len) {

    size_t i = 1;
    uint8_t frame[6];
    frame[0] = SOF_MARKER;
    frame[1] = (i << 2) + 1;
    frame[2] = FT_UIH;
    frame[3] = (len << 1) + 1;
    frame[4] = 0xFF - crc8(&frame[1], 3, FCS_POLYNOMIAL, FCS_INIT_VALUE, true);
    frame[5] = SOF_MARKER;

    term->write(frame, 4);
    term->write(data, len);
    term->write(frame + 4, 2);
    ESP_LOG_BUFFER_HEXDUMP("Send", frame, 4, ESP_LOG_INFO);
    ESP_LOG_BUFFER_HEXDUMP("Send", data, len, ESP_LOG_INFO);
    ESP_LOG_BUFFER_HEXDUMP("Send", frame+4, 2, ESP_LOG_INFO);
    return 0;
}