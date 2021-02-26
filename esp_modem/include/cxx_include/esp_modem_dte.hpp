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
 explicit dte(std::unique_ptr<terminal> terminal);
  ~dte() = default;
//  void set_line_cb(got_line f) { on_line_cb = std::move(f); }

  void set_mode(dte_mode m) { term->start(); mode = m; }
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
};




#endif //SIMPLE_CXX_CLIENT_ESP_MODEM_DTE_HPP
