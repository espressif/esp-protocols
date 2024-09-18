set(IDF_VER_DIR "idf_v$ENV{ESP_IDF_VERSION}")

idf_component_register(SRCS "smoke_test.c"
                            "${IDF_VER_DIR}/all_wifi_calls.c"
                            "${IDF_VER_DIR}/all_wifi_remote_calls.c"
                       INCLUDE_DIRS ".")
