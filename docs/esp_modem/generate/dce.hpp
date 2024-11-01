
//  --- ESP-MODEM command module starts here ---
class esp_modem::DCE : public DCE_T<GenericModule> {
public:
    using DCE_T<GenericModule>::DCE_T;

#include "esp_modem_command_declare_helper.inc"
#define ESP_MODEM_DECLARE_DCE_COMMAND(name, return_type, ...) \
    return_type name(ESP_MODEM_COMMAND_PARAMS(__VA_ARGS__));


#include "esp_modem_command_declare.inc"

#undef ESP_MODEM_DECLARE_DCE_COMMAND

};
