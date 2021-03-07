//
// Created by david on 3/8/21.
//

#ifndef SIMPLE_CXX_CLIENT_ESP_MODEM_CMUX_HPP
#define SIMPLE_CXX_CLIENT_ESP_MODEM_CMUX_HPP

enum class cmux_state {
    INIT,
    HEADER,
    PAYLOAD,
    FOOTER,
    RECOVER,
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

};


#endif //SIMPLE_CXX_CLIENT_ESP_MODEM_CMUX_HPP
