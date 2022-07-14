set(srcs esp_tls.c esp-tls-crypto/esp_tls_crypto.c esp_tls_error_capture.c esp_tls_mbedtls.c)

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS . esp-tls-crypto
                    PRIV_INCLUDE_DIRS "private_include"
                    # mbedtls is public requirements becasue esp_tls.h
                    # includes mbedtls header files.
                    REQUIRES mbedtls
                    PRIV_REQUIRES http_parser)

if(CONFIG_ESP_TLS_USING_WOLFSSL)
    idf_component_get_property(wolfssl esp-wolfssl COMPONENT_LIB)
    target_link_libraries(${COMPONENT_LIB} PUBLIC ${wolfssl})
endif()
