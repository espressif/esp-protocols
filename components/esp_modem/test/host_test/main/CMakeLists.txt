idf_component_register(SRCS "test_modem.cpp" "LoopbackTerm.cpp"
                       INCLUDE_DIRS "$ENV{IDF_PATH}/tools/catch"
                       REQUIRES esp_modem)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(${COMPONENT_LIB}  PRIVATE Threads::Threads)

target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)
target_compile_definitions(${COMPONENT_LIB} PRIVATE "-DCONFIG_IDF_TARGET_LINUX")
