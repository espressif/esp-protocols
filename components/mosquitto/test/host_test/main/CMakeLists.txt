idf_component_register(SRCS "test_mosq.cpp"
                       REQUIRES mosquitto WHOLE_ARCHIVE)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(${COMPONENT_LIB} PRIVATE Threads::Threads)

target_link_libraries(${COMPONENT_LIB} PRIVATE Catch2WithMain)

set_target_properties(${COMPONENT_LIB} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS ON
)
