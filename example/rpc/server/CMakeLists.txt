cmake_minimum_required(VERSION 3.5)

project(jr_rpc_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

aux_source_directory(src SRC_LIST)
aux_source_directory(../../../network NETWORK_SRC_LIST)

add_executable(jr_rpc_server ${SRC_LIST} ${NETWORK_SRC_LIST})

find_package(nlohmann_json 3.2.0 REQUIRED)
target_link_libraries(jr_rpc_server PUBLIC pthread PRIVATE nlohmann_json::nlohmann_json)
