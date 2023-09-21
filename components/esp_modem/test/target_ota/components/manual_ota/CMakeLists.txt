idf_component_register(SRCS manual_ota.cpp transport_batch_tls.cpp
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES extra_tcp_transports esp_http_client app_update)
