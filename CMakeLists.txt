cmake_minimum_required(VERSION 3.16)
project(MicroWriter VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)

# Ensure modern wide-character ncurses (ncursesw) is selected
set(CURSES_NEED_NCURSES TRUE)
set(CURSES_NEED_WIDE TRUE)

find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport)
find_package(Curses REQUIRED)

# 1. Main host / switchboard
file(GLOB MAIN_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/main/*.cpp")
file(GLOB MAIN_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/main/*.h")

# 2. Curses subsystem
file(GLOB CURSES_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/curses/*.cpp")
file(GLOB CURSES_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/curses/*.h")

# 3. Qt6 subsystem
file(GLOB QT6_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/qt6/*.cpp")
file(GLOB QT6_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/qt6/*.h")

# Executable target
add_executable(microwriter
    ${MAIN_SOURCES}
    ${MAIN_HEADERS}
    ${CURSES_SOURCES}
    ${CURSES_HEADERS}
    ${QT6_SOURCES}
    ${QT6_HEADERS}
)

# Expose include directories across module boundaries
target_include_directories(microwriter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/main
    ${CMAKE_CURRENT_SOURCE_DIR}/curses
    ${CMAKE_CURRENT_SOURCE_DIR}/qt6
    ${CURSES_INCLUDE_DIRS}
)

# Link Qt6 and ncursesw
target_link_libraries(microwriter PRIVATE
    Qt6::Widgets
    Qt6::PrintSupport
    ${CURSES_LIBRARIES}
)

# Arch Linux 64bit/GCC 16.2.1/CMAKE 4.4.4