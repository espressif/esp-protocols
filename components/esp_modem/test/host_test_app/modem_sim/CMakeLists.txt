cmake_minimum_required(VERSION 3.10)
project(modem_sim CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(modem_sim modem_sim.cpp)
target_link_libraries(modem_sim PRIVATE pthread)
