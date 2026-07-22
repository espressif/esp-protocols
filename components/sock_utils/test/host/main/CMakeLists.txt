idf_component_register(SRCS "test_sock_utils.cpp"
                        INCLUDE_DIRS "$ENV{IDF_PATH}/tools"
                        WHOLE_ARCHIVE)

target_compile_options(${COMPONENT_LIB} PUBLIC -fsanitize=address)
target_link_options(${COMPONENT_LIB} PUBLIC -fsanitize=address)

# IDF 6.x Linux: freertos coop syscalls define weak POSIX write/read/… which
# satisfy the linker before vfs_linux.c (strong VFS overrides) is pulled from
# libvfs.a. Without those overrides, write()/read() hit libc on lwIP FDs and
# fail with EBADF (socketpair/pipe tests). Link VFS objects explicitly so the
# strong symbols win. Not needed on IDF ≤5.5 (no coop weak stubs).
set(_coop_syscalls "$ENV{IDF_PATH}/components/freertos/FreeRTOS-Kernel/portable/linux/utils/linux_port_coop_syscalls.c")
if(EXISTS "${_coop_syscalls}")
    idf_component_get_property(vfs_lib vfs COMPONENT_LIB)
    target_link_libraries(${COMPONENT_LIB} INTERFACE "$<TARGET_OBJECTS:${vfs_lib}>")
endif()
