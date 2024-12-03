#pragma once

#define ESP_MODEM_GET_MACRO(_0, _1, _2, _3, _4, _5, _6, ESP_MODEM_MACRO_NAME, ...) ESP_MODEM_MACRO_NAME
#define ESP_MODEM_COMMAND_PARAMS(...) ESP_MODEM_GET_MACRO(_0, ##__VA_ARGS__, ESP_MODEM_HELPER6, ESP_MODEM_HELPER5, ESP_MODEM_HELPER4, ESP_MODEM_HELPER3, ESP_MODEM_HELPER2, ESP_MODEM_HELPER1, ESP_MODEM_HELPER0)(PARAM_, ESP_MODEM_NO_COMMA, ESP_MODEM_NO_COMMA, ##__VA_ARGS__)
#define ESP_MODEM_COMMAND_PARAMS_AFTER(...) ESP_MODEM_GET_MACRO(_0, ##__VA_ARGS__, ESP_MODEM_HELPER6, ESP_MODEM_HELPER5, ESP_MODEM_HELPER4, ESP_MODEM_HELPER3, ESP_MODEM_HELPER2, ESP_MODEM_HELPER1, ESP_MODEM_HELPER0)(PARAM_, ESP_MODEM_COMMA, ESP_MODEM_NO_COMMA, ##__VA_ARGS__)
#define ESP_MODEM_COMMAND_FORWARD(...) ESP_MODEM_GET_MACRO(_0, ##__VA_ARGS__, ESP_MODEM_HELPER6, ESP_MODEM_HELPER5, ESP_MODEM_HELPER4, ESP_MODEM_HELPER3, ESP_MODEM_HELPER2, ESP_MODEM_HELPER1, ESP_MODEM_HELPER0)(FORWARD_, ESP_MODEM_NO_COMMA, ESP_MODEM_NO_COMMA, ##__VA_ARGS__)
#define ESP_MODEM_COMMAND_FORWARD_AFTER(...) ESP_MODEM_GET_MACRO(_0, ##__VA_ARGS__, ESP_MODEM_HELPER6, ESP_MODEM_HELPER5, ESP_MODEM_HELPER4, ESP_MODEM_HELPER3, ESP_MODEM_HELPER2, ESP_MODEM_HELPER1, ESP_MODEM_HELPER0)(FORWARD_, ESP_MODEM_COMMA, ESP_MODEM_NO_COMMA, ##__VA_ARGS__)

#ifdef __cplusplus
#define PARAM_STR_OUT(name) std::string& name
#define PARAM_STR_IN(name) const std::string& name
#define PARAM_INT_OUT(name) int& name
#define PARAM_INT_IN(name) int name
#define PARAM_BOOL_IN(name) const bool name
#define PARAM_BOOL_OUT(name) bool& name
#define PARAM_STRUCT_OUT(struct_name, name)  struct_name& name
#define PARAM_INT_LIST_IN(name) const int* name
#else
#define PARAM_STR_OUT(name) char* name
#define PARAM_STR_IN(name)  const char* name
#define PARAM_INT_OUT(name) int* name
#define PARAM_INT_IN(name) int name
#define PARAM_BOOL_IN(name) const bool name
#define PARAM_BOOL_OUT(name) bool* name
#define PARAM_STRUCT_OUT(struct_name, name)  esp_modem_ ## struct_name ## _t* name
#define PARAM_INT_LIST_IN(name) const int* name
#endif

#define FORWARD_STR_OUT(name) name
#define FORWARD_STR_IN(name) name
#define FORWARD_INT_OUT(name) name
#define FORWARD_INT_IN(name) name
#define FORWARD_BOOL_IN(name) name
#define FORWARD_BOOL_OUT(name) name
#define FORWARD_STRUCT_OUT(struct_name, name) name
#define FORWARD_INT_LIST_IN(name) name


#define ESP_MODEM_COMMA ,
#define ESP_MODEM_NO_COMMA

#define ESP_MODEM_HELPER_EVAL(x) x
#define ESP_MODEM_HELPER_GENERIC(a, b) ESP_MODEM_HELPER_EVAL(a ## b)
#define ESP_MODEM_HELPER0(prefix, lead_comma, trail_comma)
#define ESP_MODEM_HELPER1(prefix, lead_comma, trail_comma, p1) lead_comma ESP_MODEM_HELPER_GENERIC(prefix, p1) trail_comma
#define ESP_MODEM_HELPER2(prefix, lead_comma, trail_comma, p1, p2) lead_comma ESP_MODEM_HELPER_GENERIC(prefix, p1), ESP_MODEM_HELPER_GENERIC(prefix, p2) trail_comma
#define ESP_MODEM_HELPER3(prefix, lead_comma, trail_comma, p1, p2, p3) lead_comma ESP_MODEM_HELPER_GENERIC(prefix, p1), ESP_MODEM_HELPER_GENERIC(prefix, p2), ESP_MODEM_HELPER_GENERIC(prefix, p3) trail_comma
#define ESP_MODEM_HELPER4(prefix, lead_comma, trail_comma, p1, p2, p3, p4) lead_comma \
        ESP_MODEM_HELPER_GENERIC(prefix, p1), \
        ESP_MODEM_HELPER_GENERIC(prefix, p2), \
        ESP_MODEM_HELPER_GENERIC(prefix, p3), \
        ESP_MODEM_HELPER_GENERIC(prefix, p4) trail_comma
#define ESP_MODEM_HELPER5(prefix, lead_comma, trail_comma, p1, p2, p3, p4, p5) lead_comma \
        ESP_MODEM_HELPER_GENERIC(prefix, p1), \
        ESP_MODEM_HELPER_GENERIC(prefix, p2), \
        ESP_MODEM_HELPER_GENERIC(prefix, p3), \
        ESP_MODEM_HELPER_GENERIC(prefix, p4), \
        ESP_MODEM_HELPER_GENERIC(prefix, p5) trail_comma
