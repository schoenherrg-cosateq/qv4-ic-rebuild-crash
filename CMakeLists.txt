cmake_minimum_required(VERSION 3.16)
project(qv4_ic_rebuild_crash LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Core Qml)

add_executable(qv4_ic_rebuild_crash main.cpp)
target_link_libraries(qv4_ic_rebuild_crash PRIVATE Qt6::Core Qt6::Qml)
