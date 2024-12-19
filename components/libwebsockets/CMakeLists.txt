idf_component_register()

option(LWS_WITH_EXPORT_LWSTARGETS "Export libwebsockets CMake targets.  Disable if they conflict with an outer cmake project." OFF)
set(LWS_WITH_EXPORT_LWSTARGETS OFF)

target_link_libraries(${COMPONENT_LIB} INTERFACE websockets)


add_subdirectory(libwebsockets)
