//  --- DCE command documentation starts here ---

class DCE: public DCE_T<GenericModule> {
public:
    using DCE_T<GenericModule>::DCE_T;






/**
 * @brief Sends the supplied PIN code
 *
 * @param pin Pin
 *
 */ command_result set_pin (const std::string& pin); /**
 * @brief Checks if the SIM needs a PIN
 *
 * @param[out] pin_ok Pin
 */ command_result read_pin (bool& pin_ok); /**
 * @brief Reads the module name
 *
 * @param[out] name module name
 */ command_result set_echo (const bool x); /**
 * @brief Reads the module name
 *
 * @param[out] name module name
 */ command_result resume_data_mode (); /**
 * @brief Reads the module name
 *
 * @param[out] name module name
 */ command_result set_pdp_context (PdpContext& x); /**
 * @brief Reads the module name
 *
 * @param[out] name module name
 */ command_result set_command_mode (); /**
 * @brief Reads the module name
 *
 * @param[out] name module name
 */ command_result set_cmux (); /**
 * @brief Reads the module name
 *
 * @param[out] name module name
 */ command_result get_imsi (std::string& x); /**
 * @brief Reads the module name
 *
 * @param[out] name module name
 */ command_result get_imei (std::string& x); /**
 * @brief Reads the module name
 *
 * @param[out] name module name
 */ command_result get_module_name (std::string& name); /**
 * @brief Sets the modem to data mode
 *
 */ command_result set_data_mode (); /**
 * @brief Get Signal quality
 *
 */ command_result get_signal_quality (int& x, int& y);
};
