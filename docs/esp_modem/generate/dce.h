
//  --- ESP-MODEM command module starts here ---
#include "esp_modem_command_declare_helper.inc"
#define ESP_MODEM_DECLARE_DCE_COMMAND(name, return_type, ...) return_type esp_modem_ ## name (ESP_MODEM_COMMAND_PARAMS(__VA_ARGS__));

#include "esp_modem_command_declare.inc"
