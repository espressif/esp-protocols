#include "cxx_include/esp_modem_commands.hpp"
#include "cxx_include/esp_modem_dte.hpp"
#include <string>
#include <utility>

namespace esp_modem::dce_commands {



//template <typename T> static inline command_result generic_command(T t, std::string command,
//                                                    std::string pass_phrase, std::string fail_phrase, uint32_t timeout_ms)
//{
//    return t->command(command.c_str(), [&](uint8_t *data, size_t len) {
//        std::string response((char*)data, len);
//        if (response.find(pass_phrase) != std::string::npos) {
//            return command_result::OK;
//        } else if (response.find(fail_phrase) != std::string::npos) {
//            return command_result::FAIL;
//        }
//        return command_result::TIMEOUT;
//    }, timeout_ms);
//}
//
//template <typename T> static inline command_result generic_command_common(T t, std::string command)
//{
//    return generic_command(t, command, "OK", "ERROR", 500);
//}
//

//template <typename T> command_result sync(T t)
//{
//    return t->command("AT\r", [&](uint8_t *data, size_t len) {
//        std::string response((char*)data, len);
//        if (response.find("OK") != std::string::npos) {
//            return command_result::OK;
//        } else if (response.find("ERROR") != std::string::npos) {
//            return command_result::FAIL;
//        }
//        return command_result::TIMEOUT;
//    }, 1000);
//}

//template <typename T> command_result set_echo(T t, bool on) { return command_result::OK; }



//template <typename T> command_result set_echo(T* t, bool on)
//{
//    if (on)
//        return generic_command_common(t, "ATE1\r");
//    return generic_command_common(t, "ATE0\r");
//}
//
//template <typename T> command_result set_data_mode(T t)
//{
//    return generic_command(t, "ATD*99##\r", "CONNECT", "ERROR", 5000);
//}
//
//template <typename T> command_result set_pdp_context(T t, PdpContext& pdp_context)
//{
//    return generic_command(t, "ATD*99##\r", "CONNECT", "ERROR", 5000);
//}

}

namespace esp_modem {

template <class T>
class generic_dce {
public:
    explicit generic_dce(std::shared_ptr<DTE> e) : dce_dte(std::move(e)) {}

    esp_err_t sync() { return dce_commands::sync(dce_dte); }

private:
    std::shared_ptr<T> dce_dte;
};

}