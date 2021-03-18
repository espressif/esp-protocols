//
// Created by david on 3/16/21.
//

#ifndef MODEM_CONSOLE_EXCEPTION_STUB_HPP
#define MODEM_CONSOLE_EXCEPTION_STUB_HPP

#ifdef CONFIG_COMPILER_CXX_EXCEPTIONS
#define TRY_CATCH_RET_NULL(block) \
    try { block                   \
    } catch (std::bad_alloc& e) { \
        ESP_LOGE(TAG, "Out of memory"); \
        return nullptr; \
    } catch (esp_err_exception& e) {    \
        esp_err_t err = e.get_err_t();  \
        ESP_LOGE(TAG, "Error occurred during UART term init: %d", err); \
        ESP_LOGE(TAG, "%s", e.what());  \
        return nullptr; \
    }
#else
#define TRY_CATCH_RET_NULL(block) \
    block
#endif


#endif //MODEM_CONSOLE_EXCEPTION_STUB_HPP
