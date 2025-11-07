cmake_minimum_required(VERSION 3.15)

project(CyberSeagull4Server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(CyberSeagull4Server
	src/Server.cpp)

target_include_directories(CyberSeagull4Server PUBLIC include)
target_link_libraries(CyberSeagull4Server PRIVATE libremid)
