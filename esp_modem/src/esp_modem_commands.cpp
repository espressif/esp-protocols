#include "cxx_include/esp_modem_commands.hpp"
#include "cxx_include/esp_modem_dte.hpp"
#include <string>
#include <utility>

namespace esp_modem::dce_commands {

template <typename T> esp_err_t sync(T t)
{
    return t->send_command("AT\r", [&](uint8_t *data, size_t len) {
        std::string response((char*)data, len);
        if (response.find("OK") != std::string::npos) {
            return ESP_OK;
        } else if (response.find("ERROR") != std::string::npos) {
            return ESP_FAIL;
        }
        return ESP_ERR_INVALID_STATE;
    }, 1000);
}

}

namespace esp_modem {

template <class T>
class generic_dce {
public:
    explicit generic_dce(std::shared_ptr<dte> e) : dce_dte(std::move(e)) {}

    esp_err_t sync() { return dce_commands::sync(dce_dte); }

private:
    std::shared_ptr<T> dce_dte;
};

}