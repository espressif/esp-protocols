idf_component_register(SRCS freertos_linux.c
                            osal/queue.cpp osal/event_group.cpp osal/mutex.cpp
                       INCLUDE_DIRS include)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(${COMPONENT_LIB} PRIVATE Threads::Threads)

set_target_properties(${COMPONENT_LIB} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS ON
)
