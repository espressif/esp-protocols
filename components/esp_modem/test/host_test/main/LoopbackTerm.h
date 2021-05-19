#ifndef _LOOPBACKTERM_H_
#define _LOOPBACKTERM_H_

#include "cxx_include/esp_modem_api.hpp"
#include "cxx_include/esp_modem_terminal.hpp"

using namespace esp_modem;

class LoopbackTerm : public Terminal {
public:
    explicit LoopbackTerm(bool is_bg96);
    explicit LoopbackTerm();

    ~LoopbackTerm() override;

    void start() override;
    void stop() override;

    int write(uint8_t *data, size_t len) override;

    int read(uint8_t *data, size_t len) override;

private:
    enum class status_t {
        STARTED,
        STOPPED
    };
    status_t status;
    SignalGroup signal;
    std::vector<uint8_t> loopback_data;
    size_t data_len;
    bool pin_ok;
    bool is_bg96;
};

#endif //_LOOPBACKTERM_H_
