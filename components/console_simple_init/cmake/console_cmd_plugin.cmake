# Usage, after idf_component_register() in the plugin component:
#   idf_component_get_property(console_dir console_simple_init COMPONENT_DIR)
#   include(${console_dir}/cmake/console_cmd_plugin.cmake)
#   console_cmd_plugin_force_link(${COMPONENT_LIB} console_cmd_wifi_register)
#
# register_symbol is the global register function, not the static descriptor.
# -u pulls that object file in; linker.lf KEEP() retains .console_cmd_desc.

function(console_cmd_plugin_force_link component_lib register_symbol)
    target_link_libraries(${component_lib} "-u ${register_symbol}")
endfunction()
