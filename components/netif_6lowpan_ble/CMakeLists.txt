idf_component_register(
  SRCS
    src/debug_print_utils.c
    src/lowpan6_ble.c
    src/lowpan6_ble_netif.c
  INCLUDE_DIRS
    include
  PRIV_INCLUDE_DIRS
    src
  REQUIRES
    bt
    esp_netif
)

idf_component_get_property(lwip lwip COMPONENT_LIB)
target_sources(
    ${lwip}
    PRIVATE
    "$ENV{IDF_PATH}/components/lwip/lwip/src/netif/lowpan6.c"
    "$ENV{IDF_PATH}/components/lwip/lwip/src/netif/lowpan6_ble.c"
    "$ENV{IDF_PATH}/components/lwip/lwip/src/netif/lowpan6_common.c"
)
