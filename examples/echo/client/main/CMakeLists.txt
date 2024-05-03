idf_component_register(
  SRCS
    main.c
  REQUIRES
    lowpan6_ble
    nvs_flash
)

idf_component_get_property(lwip lwip COMPONENT_LIB)
target_compile_options(${lwip} PRIVATE -Wno-error=format)
