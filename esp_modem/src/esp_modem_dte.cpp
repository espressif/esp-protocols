#include "cxx_include/esp_modem_dte.hpp"
#include <string.h>
#include "esp_log.h"

DTE::DTE(std::unique_ptr<Terminal> terminal):
        buffer_size(DTE_BUFFER_SIZE), consumed(0),
        buffer(std::make_unique<uint8_t[]>(buffer_size)),
        term(std::move(terminal)), mode(dte_mode::UNDEF) {}

command_result DTE::command(const std::string& command, got_line_cb got_line, uint32_t time_ms)
{
    command_result res = command_result::TIMEOUT;
    term->write((uint8_t *)command.c_str(), command.length());
    term->set_data_cb([&](size_t len){
        auto data_to_read = std::min(len, buffer_size - consumed);
        auto data = buffer.get() + consumed;
        auto actual_len = term->read(data, data_to_read);
        consumed += actual_len;
        if (memchr(data, '\n', actual_len)) {
//            ESP_LOGE("GOT", "LINE");
            res = got_line(buffer.get(), consumed);
            if (res == command_result::OK || res == command_result::FAIL) {
                signal.set(GOT_LINE);
                return true;
            }
        }
        return false;
    });
    auto got_lf = signal.wait(GOT_LINE, time_ms);
    if (got_lf && res == command_result::TIMEOUT) {
        throw_if_esp_fail(ESP_ERR_INVALID_STATE);
    }
    consumed = 0;
    term->set_data_cb(nullptr);
    return res;
}