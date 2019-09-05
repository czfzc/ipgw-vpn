cmake_minimum_required(VERSION 3.10)
project(ipgw_flooder)
#find_package(curl REQUIRED)
set(CMAKE_CXX_STANDARD 11)

add_executable(ipgw_flooder main.cpp http_socket_req.cpp req_res.cpp flood.cpp parse_option.cpp easy_socket/Socket.cpp)
#target_link_libraries(ipgw_flooder curl)