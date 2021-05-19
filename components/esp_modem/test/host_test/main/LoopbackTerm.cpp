#include <memory>
#include <future>
#include <cstring>
#include "LoopbackTerm.h"

void LoopbackTerm::start()
{
    status = status_t::STARTED;
}

void LoopbackTerm::stop()
{
    status = status_t::STOPPED;
}

int LoopbackTerm::write(uint8_t *data, size_t len)
{
    if (len > 2 && (data[len-1] == '\r' || data[len-1] == '+') ) { // Simple AT responder
        std::string command((char*)data, len);
        std::string response;
        if (command == "+++") {
            response = "NO CARRIER\r\n";
        } else if (command == "ATE1\r" || command == "ATE0\r") {
            response = "OK\r\n";
        } else if (command == "ATO\r") {
            response = "ERROR\r\n";
        } else if (command.find("ATD") != std::string::npos) {
            response = "CONNECT\r\n";
        } else if (command.find("AT+CSQ\r") != std::string::npos) {
            response = "+CSQ: 123,456\n\r\nOK\r\n";
        } else if (command.find("AT+CBC\r") != std::string::npos) {
            response = is_bg96 ? "+CBC: 1,2,123456V\r\r\n\r\nOK\r\n\n\r\n":
                                 "+CBC: 123.456V\r\r\n\r\nOK\r\n\n\r\n";
        } else if (command.find("AT+CPIN=1234\r") != std::string::npos) {
            response = "OK\r\n";
            pin_ok = true;
        } else if (command.find("AT+CPIN?\r") != std::string::npos) {
            response = pin_ok?"+CPIN: READY\r\nOK\r\n":"+CPIN: SIM PIN\r\nOK\r\n";
        } else if (command.find("AT") != std::string::npos) {
            response = "OK\r\n";
        }
        if (!response.empty()) {
            data_len = response.length();
            loopback_data.resize(data_len);
            memcpy(&loopback_data[0], &response[0], data_len);
            auto ret = std::async(on_data, nullptr, data_len);
            return len;
        }
    }
    if (len > 2 && data[0] == 0xf9) { // Simple CMUX responder
        // turn the request into a reply -> implements CMUX loopback
        if (data[2] == 0x3f)    // SABM command
            data[2] = 0x73;
        else if (data[2] == 0xef) { // Generic request
            data[2] = 0xff;         // generic reply
        }
    }
    loopback_data.resize(data_len + len);
    memcpy(&loopback_data[data_len], data, len);
    data_len += len;
    auto ret = std::async(on_data, nullptr, data_len);
    return len;
}

int LoopbackTerm::read(uint8_t *data, size_t len)
{
    size_t read_len = std::min(data_len, len);
    if (read_len) {
        if (loopback_data.capacity() < len)
            loopback_data.reserve(len);
        memcpy(data, &loopback_data[0], read_len);
        loopback_data.erase(loopback_data.begin(), loopback_data.begin() + read_len);
        data_len -= read_len;
    }
    return read_len;
}

LoopbackTerm::LoopbackTerm(bool is_bg96): loopback_data(), data_len(0), pin_ok(false), is_bg96(is_bg96) {}

LoopbackTerm::LoopbackTerm(): loopback_data(), data_len(0), pin_ok(false), is_bg96(false) {}

LoopbackTerm::~LoopbackTerm() = default;
