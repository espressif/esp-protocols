idf_component_register(
    SRCS additional_commands.c
    INCLUDE_DIRS include
    REQUIRES at freertos nvs_flash)

idf_component_set_property(${COMPONENT_NAME} WHOLE_ARCHIVE TRUE)
