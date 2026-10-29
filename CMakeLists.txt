cmake_minimum_required(VERSION 3.16)
project(MicroWriter VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport)
find_package(Curses REQUIRED)

set(CMAKE_AUTOMOC ON)

# 1. Main host / switchboard
file(GLOB MAIN_SOURCES CONFIGURE_DEPENDS "main/*.cpp")
file(GLOB MAIN_HEADERS CONFIGURE_DEPENDS "main/*.h")

# 2. Curses subsystem
file(GLOB CURSES_SOURCES CONFIGURE_DEPENDS "curses/*.cpp")
file(GLOB CURSES_HEADERS CONFIGURE_DEPENDS "curses/*.h")

# 3. Qt6 subsystem
file(GLOB QT6_SOURCES CONFIGURE_DEPENDS "qt6/*.cpp")
file(GLOB QT6_HEADERS CONFIGURE_DEPENDS "qt6/*.h")

# Ensure every variable is distinctly space-separated
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
    main
    curses 
    qt6 
    ${CURSES_INCLUDE_DIRS}
)

target_link_libraries(microwriter PRIVATE 
    Qt6::Widgets 
    Qt6::PrintSupport 
    ${CURSES_LIBRARIES}
)

# Arch Linux 64bit/GCC 16.2.1/CMAKE 4.4.4