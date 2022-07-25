set(FREERTOS_DIR $ENV{EXT_FREERTOS_DIR})
if(NOT DEFINED FREERTOS_DIR)
    message(FATAL_ERROR "Please define EXT_FREERTOS_DIR (run ./install.sh)")
endif()

set(srcs    # Kernel files
            ${FREERTOS_DIR}/croutine.c
            ${FREERTOS_DIR}/event_groups.c
            ${FREERTOS_DIR}/queue.c
            ${FREERTOS_DIR}/stream_buffer.c
            ${FREERTOS_DIR}/tasks.c
            ${FREERTOS_DIR}/list.c
            ${FREERTOS_DIR}/timers.c
            # GCC-Posix port files
            wait_for_event.c
            port.c
            heap_3.c
            # ESP's default main with hooks and scheduler's entry point
            default_main.c
            )

idf_component_register(SRCS ${srcs}
                            esp_stubs/esp_stubs.c
                       INCLUDE_DIRS . include include/stubs include/freertos include/utils ${FREERTOS_DIR}/include)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(${COMPONENT_LIB} PRIVATE Threads::Threads)
