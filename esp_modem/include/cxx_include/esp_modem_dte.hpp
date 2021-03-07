#ifndef SIMPLE_CXX_CLIENT_ESP_MODEM_DTE_HPP
#define SIMPLE_CXX_CLIENT_ESP_MODEM_DTE_HPP
#include <memory>
#include <functional>
#include <exception>
#include <cstddef>
#include <cstdint>
#include <utility>
#include "esp_err.h"
#include "cxx_include/esp_modem_primitives.hpp"
#include "cxx_include/esp_modem_terminal.hpp"
#include "cxx_include/esp_modem_cmux.hpp"
#include "cxx_include/esp_modem_types.hpp"


const int DTE_BUFFER_SIZE = 1024;


class DTE: public CommandableIf {
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
  void set_mode(modem_mode m) {
        term->start(); mode = m;
        if (m == modem_mode::DATA_MODE) {
            term->set_data_cb(on_data);
        } else if (m == modem_mode::CMUX_MODE) {
            setup_cmux();
        }
    }
    command_result command(const std::string& command, got_line_cb got_line, uint32_t time_ms) override;
//    std::shared_ptr<uint8_t[]> buffer;

    void send_cmux_command(uint8_t i, const std::string& command);
private:
    Lock lock;
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
    modem_mode mode;
    signal_group signal;
    std::function<bool(size_t len)> on_data;


};






#endif //SIMPLE_CXX_CLIENT_ESP_MODEM_DTE_HPP
