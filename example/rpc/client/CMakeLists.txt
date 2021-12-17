cmake_minimum_required(VERSION 3.5)

project(jr_rpc_client LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(nlohmann_json 3.2.0 REQUIRED)
aux_source_directory(src SRC_LIST)
add_executable(jr_rpc_client ${SRC_LIST} ../../../network/socket.cpp ../../../network/buffer.cpp)
target_link_libraries(jr_rpc_client PRIVATE nlohmann_json::nlohmann_json)
