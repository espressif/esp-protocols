#ifndef SIMPLE_CXX_CLIENT_ESP_MODEM_DTE_HPP
#define SIMPLE_CXX_CLIENT_ESP_MODEM_DTE_HPP

#include <memory>
#include <functional>
#include <exception>
#include <cstddef>
#include <cstdint>
#include <utility>
#include "esp_err.h"
#include "terminal_objects.hpp"
#include "ppp_netif.hpp"


enum class terminal_error {
    BUFFER_OVERFLOW,
    CHECKSUM_ERROR,
    UNEXPECTED_CONTROL_FLOW,
};

class terminal {
public:
    virtual ~terminal() = default;
    void set_data_cb(std::function<void(size_t len)> f) { on_data = std::move(f); }
    void set_error_cb(std::function<void(terminal_error)> f) { on_error = std::move(f); }
    virtual int write(uint8_t *data, size_t len) = 0;
    virtual int read(uint8_t *data, size_t len) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;

protected:
    std::function<void(size_t len)> on_data;
    std::function<void(terminal_error)> on_error;
};

class dte_adapter: public terminal {
public:
    dte_adapter(std::unique_ptr<terminal> terminal):
            original_dte(std::move(terminal)) {}
    ~dte_adapter() override = default;
    int write(uint8_t *data, size_t len) override { return original_dte->write(data, len); }
    int read(uint8_t *data, size_t len)  override { return original_dte->read(data, len); }
private:
    std::unique_ptr<terminal> original_dte;
};


enum class dte_mode {
    UNDEF,
    COMMAND_MODE,
    DATA_MODE
};

const int DTE_BUFFER_SIZE = 1024;

struct dte_data {
    uint8_t *data;
    size_t len;
};

typedef std::function<bool(uint8_t *data, size_t len)> got_line_cb;

class dte {
public:
 explicit dte(std::unique_ptr<terminal> t);
  ~dte() = default;
//  void set_line_cb(got_line f) { on_line_cb = std::move(f); }
    int write(uint8_t *data, size_t len)  { return term->write(data, len); }
    int read(uint8_t **d, size_t len)   {
        auto data_to_read = std::min(len, buffer_size);
        auto data = buffer.get();
        auto actual_len = term->read(data, data_to_read);
        *d = data;
        return actual_len;
    }
    void set_data_cb(std::function<void(size_t len)> f) { on_data = std::move(f); }

    void start() { term->start(); }
    void data_mode_closed() { term->stop(); }
  void set_mode(dte_mode m) {
        term->start(); mode = m;
        if (m == dte_mode::DATA_MODE) {
            term->set_data_cb(on_data);
        }
    }
  bool send_command(const std::string& command, got_line_cb got_line, uint32_t time_ms);

private:
    const size_t GOT_LINE = BIT0;
    size_t buffer_size;
    size_t consumed;
    std::unique_ptr<uint8_t[]> buffer;
    std::unique_ptr<terminal> term;
    got_line_cb on_line;
    dte_mode mode;
    signal_group signal;
    std::function<void(size_t len)> on_data;
};


class dce {
public:
    explicit dce(std::shared_ptr<dte> d, esp_netif_t * netif);
    void set_data() {
        command("AT+CGDCONT=1,\"IP\",\"internet\"\r", [&](uint8_t *data, size_t len) {
            return true;
        }, 1000);
        command("ATD*99***1#\r", [&](uint8_t *data, size_t len) {
            return true;
        }, 1000);

        dce_dte->set_mode(dte_mode::DATA_MODE);
        ppp_netif.start();
    }
    bool command(const std::string& command, got_line_cb got_line, uint32_t time_ms) {
        return dce_dte->send_command(command, got_line, time_ms);
    }
private:
    std::shared_ptr<dte> dce_dte;
    ppp ppp_netif;
};



#endif //SIMPLE_CXX_CLIENT_ESP_MODEM_DTE_HPP
