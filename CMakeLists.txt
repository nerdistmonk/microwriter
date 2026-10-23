cmake_minimum_required(VERSION 3.16)
project(MicroWriter VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport)

set(CMAKE_AUTOMOC ON)

add_executable(microwriter main.cpp)

target_link_libraries(microwriter PRIVATE Qt6::Widgets Qt6::PrintSupport)
