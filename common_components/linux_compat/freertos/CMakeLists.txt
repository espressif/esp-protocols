idf_build_get_property(idf_path IDF_PATH)
idf_build_get_property(idf_target IDF_TARGET)

set(freertos_linux_port "${idf_path}/components/freertos/FreeRTOS-Kernel/portable/linux")
set(coop_syscalls "${freertos_linux_port}/utils/linux_port_coop_syscalls.c")

# Blocking host syscalls stall the FreeRTOS Linux simulator; redirect a small
# set of libc entry points to the IDF cooperative layer via linker wrap.
set(coop_wrap_functions socket accept fcntl close send read poll)

set(srcs freertos_linux.c
         osal/queue.cpp
         osal/event_group.cpp
         osal/mutex.cpp)
set(include_dirs include)
set(priv_include_dirs "")

# Cooperative I/O wrappers were added in IDF 6.x for the Linux VFS layer.
if("${idf_target}" STREQUAL "linux" AND EXISTS "${coop_syscalls}")
    list(APPEND srcs "${coop_syscalls}" linux_coop_linker_wrap.c)
    list(APPEND include_dirs "${freertos_linux_port}/include")
    list(APPEND priv_include_dirs "${freertos_linux_port}/utils")
endif()

if(priv_include_dirs)
    idf_component_register(SRCS ${srcs}
                           INCLUDE_DIRS ${include_dirs}
                           PRIV_INCLUDE_DIRS ${priv_include_dirs}
                           REQUIRES heap)
else()
    idf_component_register(SRCS ${srcs}
                           INCLUDE_DIRS ${include_dirs}
                           REQUIRES heap)
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(${COMPONENT_LIB} PRIVATE Threads::Threads)
if("${idf_target}" STREQUAL "linux" AND EXISTS "${coop_syscalls}")
    target_link_libraries(${COMPONENT_LIB} PRIVATE dl)
    foreach(wrap ${coop_wrap_functions})
        target_link_options(${COMPONENT_LIB} INTERFACE
            "-Wl,--wrap=${wrap}"
            "-Wl,-u,__wrap_${wrap}")
    endforeach()
endif()

set_target_properties(${COMPONENT_LIB} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS ON
)
