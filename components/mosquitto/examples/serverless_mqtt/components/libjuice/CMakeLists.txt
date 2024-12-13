set(LIBJUICE_VERSION "73785387eafe15c02b6a210edb10f722474e8e14")
set(LIBJUICE_URL "https://github.com/paullouisageneau/libjuice/archive/${LIBJUICE_VERSION}.zip")

set(libjuice_dir ${CMAKE_BINARY_DIR}/libjuice/libjuice-${LIBJUICE_VERSION})

# Fetch the library
if(NOT EXISTS ${libjuice_dir})
        message(STATUS "Downloading libjuice ${LIBJUICE_VERSION}...")
        file(DOWNLOAD ${LIBJUICE_URL} ${CMAKE_BINARY_DIR}/libjuice.zip SHOW_PROGRESS)
        execute_process(COMMAND unzip -o ${CMAKE_BINARY_DIR}/libjuice.zip -d ${CMAKE_BINARY_DIR}/libjuice
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endif()

set(JUICE_SOURCES   ${libjuice_dir}/src/addr.c
                    ${libjuice_dir}/src/agent.c
                    ${libjuice_dir}/src/base64.c
                    ${libjuice_dir}/src/conn.c
                    ${libjuice_dir}/src/conn_mux.c
                    ${libjuice_dir}/src/conn_poll.c
                    ${libjuice_dir}/src/conn_thread.c
                    ${libjuice_dir}/src/const_time.c
                    ${libjuice_dir}/src/crc32.c
                    ${libjuice_dir}/src/hash.c
                    ${libjuice_dir}/src/ice.c
                    ${libjuice_dir}/src/juice.c
                    ${libjuice_dir}/src/log.c
                    ${libjuice_dir}/src/server.c
                    ${libjuice_dir}/src/stun.c
                    ${libjuice_dir}/src/timestamp.c
                    ${libjuice_dir}/src/turn.c
                    ${libjuice_dir}/src/udp.c
# Use hmac from mbedtls and random numbers from esp_random:
#                    ${libjuice_dir}/src/hmac.c
#                    ${libjuice_dir}/src/random.c
        )

idf_component_register(SRCS     port/juice_random.c
                                ${JUICE_SOURCES}
                       INCLUDE_DIRS "include"  "${libjuice_dir}/include" "${libjuice_dir}/include/juice"
                       REQUIRES esp_netif
                       PRIV_REQUIRES sock_utils)

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
set_source_files_properties(${libjuice_dir}/src/udp.c PROPERTIES COMPILE_FLAGS -Wno-unused-variable)
