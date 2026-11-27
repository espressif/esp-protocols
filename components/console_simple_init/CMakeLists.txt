set(srcs "console_simple_init.c")

if(CONFIG_CONSOLE_SIMPLE_INIT_ENABLE_STOP)
    list(APPEND srcs "console_simple_init_stop.c")
endif()

idf_component_register(SRCS ${srcs}
                    INCLUDE_DIRS "."
                    PRIV_INCLUDE_DIRS "private_include"
                    REQUIRES console
                    LDFRAGMENTS linker.lf)
