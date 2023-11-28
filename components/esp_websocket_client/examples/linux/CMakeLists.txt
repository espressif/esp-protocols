cmake_minimum_required(VERSION 3.5)

set(COMPONENTS esp_websocket_client main)
set(common_component_dir ../../../../common_components)
set(EXTRA_COMPONENT_DIRS
   ../../..
  "${common_component_dir}/linux_compat/esp_timer"
  "${common_component_dir}/linux_compat"
  "${common_component_dir}/linux_compat/freertos")

list(APPEND EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/common_components/protocol_examples_common)
list(APPEND EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/protocols/linux_stubs/esp_stubs)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(websocket)
