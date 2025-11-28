# Conditionally require components based on target
if(${IDF_TARGET} STREQUAL "linux")
    idf_component_register(SRCS "autobahn_testee.c"
                           INCLUDE_DIRS "."
                           REQUIRES esp_websocket_client protocol_examples_common esp_event esp_netif)
else()
    idf_component_register(SRCS "autobahn_testee.c"
                           INCLUDE_DIRS "."
                           REQUIRES esp_websocket_client protocol_examples_common nvs_flash esp_wifi esp_event)
endif()
