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
#include <array>

enum class terminal_error {
    BUFFER_OVERFLOW,
    CHECKSUM_ERROR,
    UNEXPECTED_CONTROL_FLOW,
};

class Terminal {
public:
    virtual ~Terminal() = default;
    virtual void set_data_cb(std::function<bool(size_t len)> f) { on_data = std::move(f); }
    void set_error_cb(std::function<void(terminal_error)> f) { on_error = std::move(f); }
    virtual int write(uint8_t *data, size_t len) = 0;
    virtual int read(uint8_t *data, size_t len) = 0;
    virtual void start() = 0;
    virtual void stop() = 0;

protected:
    std::function<bool(size_t len)> on_data;
    std::function<void(terminal_error)> on_error;
};


enum class cmux_state {
    INIT,
    HEADER,
    PAYLOAD,
    FOOTER,

};

class CMUXedTerminal: public Terminal {
public:
    explicit CMUXedTerminal(std::unique_ptr<Terminal> t, std::unique_ptr<uint8_t[]> b, size_t buff_size):
            term(std::move(t)), buffer(std::move(b)) {}
    ~CMUXedTerminal() override = default;
    void setup_cmux();
    void set_data_cb(std::function<bool(size_t len)> f) override {}
    int write(uint8_t *data, size_t len) override;
    int read(uint8_t *data, size_t len)  override { return term->read(data, len); }
    void start() override;
    void stop() override {}
private:
    static bool process_cmux_recv(size_t len);
    void send_sabm(size_t i);
    std::unique_ptr<Terminal> term;
    cmux_state state;
    uint8_t dlci;
    uint8_t type;
    size_t payload_len;
    uint8_t frame_header[6];
    size_t frame_header_offset;
    size_t buffer_size;
    size_t consumed;
    std::unique_ptr<uint8_t[]> buffer;
    bool on_cmux(size_t len);
    static bool s_on_cmux(size_t len);

};


enum class dte_mode {
    UNDEF,
    COMMAND_MODE,
    DATA_MODE,
    CMUX_MODE
};

enum class command_result {
    OK,
    FAIL,
    TIMEOUT
};

const int DTE_BUFFER_SIZE = 1024;


struct CMUXHelper {
    void send_sabm(size_t dlci);
};



typedef std::function<command_result(uint8_t *data, size_t len)> got_line_cb;

//class CMUX {
//public:
//    void setup_cmux();
//    void send_sabm(size_t dlci);
//
//    bool on_cmux(size_t len);
//    static bool s_on_cmux(size_t len);
//    cmux_state state;
//    uint8_t dlci;
//    uint8_t type;
//    size_t payload_len;
//    uint8_t frame_header[6];
//    size_t frame_header_offset;
//    size_t buffer_size;
//    size_t consumed;
////    std::shared_ptr<std::vector<uint8_t>> buffer;
//    std::unique_ptr<uint8_t[]> buffer;
//
//};

class DTE {
public:
 explicit DTE(std::unique_ptr<Terminal> t);
  ~DTE() = default;

//    std::unique_ptr<Terminal> detach() { return std::move(term); }
//    void attach(std::unique_ptr<Terminal> t) { term = std::move(t); }
//  void set_line_cb(got_line f) { on_line_cb = std::move(f); }
    int write(uint8_t *data, size_t len)  { return term->write(data, len); }
    int read(uint8_t **d, size_t len)   {
        auto data_to_read = std::min(len, buffer_size);
        auto data = buffer.get();
        auto actual_len = term->read(data, data_to_read);
        *d = data;
        return actual_len;
    }
    void set_data_cb(std::function<bool(size_t len)> f)
    {
//        on_data = std::move(f);
        term->set_data_cb(std::move(f));

    }
//    std::shared_ptr<uint8_t[]> get_buffer() { return buffer;}
    void start() { term->start(); }
    void data_mode_closed() { term->stop(); }
  void set_mode(dte_mode m) {
        term->start(); mode = m;
        if (m == dte_mode::DATA_MODE) {
            term->set_data_cb(on_data);
        } else if (m == dte_mode::CMUX_MODE) {
            setup_cmux();
        }
    }
    command_result command(const std::string& command, got_line_cb got_line, uint32_t time_ms);
//    std::shared_ptr<uint8_t[]> buffer;

    void send_cmux_command(uint8_t i, const std::string& command);
private:
//    std::unique_ptr<CMUXedTerminal> cmux;
    void setup_cmux();
//    void send_sabm(size_t dlci);
//    CMUXHelper cmux;
    static const size_t GOT_LINE = BIT0;
    size_t buffer_size;
    size_t consumed;
//    std::shared_ptr<std::vector<uint8_t>> buffer;
    std::unique_ptr<uint8_t[]> buffer;
    std::unique_ptr<Terminal> term;
//    got_line_cb on_line;
    dte_mode mode;
    signal_group signal;
    std::function<bool(size_t len)> on_data;


};






#endif //SIMPLE_CXX_CLIENT_ESP_MODEM_DTE_HPP
