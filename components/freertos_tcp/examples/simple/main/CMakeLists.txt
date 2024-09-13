#set(fdir /home/david/esp/idf/components/freertos/FreeRTOS-Kernel/include/freertos)
#set(wdir /home/david/esp/idf/components/esp_wifi/include)
#set(fcdir /home/david/esp/idf/components/freertos/config/include/freertos)
#set(dir /home/david/temp/FreeRTOS-Plus-TCP/source)
#set(idir ${dir}/include ${dir}/portable/Compiler/GCC/)


idf_component_register(SRCS "tcp_client.c"
                    PRIV_REQUIRES freertos_tcp
#                    PRIV_INCLUDE_DIRS ${fdir} ${fcdir} ${idir} ${wdir} .
                    INCLUDE_DIRS "")

#target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
