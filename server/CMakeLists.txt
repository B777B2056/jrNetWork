cmake_minimum_required(VERSION 3.5)

project(jr_rpc_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(jr_rpc_server main.cpp ../network/log.cpp ../network/timer.cpp ../network/socket.cpp ../network/buffer.cpp)

find_package(nlohmann_json 3.2.0 REQUIRED)
target_link_libraries(jr_rpc_server PUBLIC pthread PRIVATE nlohmann_json::nlohmann_json)
