//
// Created by david on 3/8/21.
//

#ifndef SIMPLE_CXX_CLIENT_ESP_MODEM_TYPES_HPP
#define SIMPLE_CXX_CLIENT_ESP_MODEM_TYPES_HPP


enum class modem_mode {
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

typedef std::function<command_result(uint8_t *data, size_t len)> got_line_cb;

struct PdpContext {
    explicit PdpContext(std::string& apn): context_id(1), protocol_type("IP"), apn(apn) {}
    explicit PdpContext(const char *apn): context_id(1), protocol_type("IP"), apn(apn) {}
    size_t context_id;
    std::string protocol_type;
    std::string apn;
};

class CommandableIf {
public:
    virtual command_result command(const std::string& command, got_line_cb got_line, uint32_t time_ms) = 0;
};

class ModuleIf {
public:
    virtual bool setup_data_mode() = 0;
    virtual bool set_mode(modem_mode mode) = 0;
};

#endif //SIMPLE_CXX_CLIENT_ESP_MODEM_TYPES_HPP
