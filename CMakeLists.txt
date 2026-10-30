cmakeminimumrequired(VERSION 3.16)
project(MicroWriter VERSION 1.0 LANGUAGES CXX)

set(CMAKECXXSTANDARD 17)
set(CMAKECXXSTANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets PrintSupport)
find_package(Curses REQUIRED)

set(CMAKE_AUTOMOC ON)

#1. Main host / switchboard
file(GLOB MAINSOURCES CONFIGUREDEPENDS "main/*.cpp")
file(GLOB MAINHEADERS CONFIGUREDEPENDS "main/*.h")

#2. Curses subsystem
file(GLOB CURSESSOURCES CONFIGUREDEPENDS "curses/*.cpp")
file(GLOB CURSESHEADERS CONFIGUREDEPENDS "curses/*.h")

#3. Qt6 subsystem
file(GLOB QT6SOURCES CONFIGUREDEPENDS "qt6/*.cpp")
file(GLOB QT6HEADERS CONFIGUREDEPENDS "qt6/*.h")

#Ensure every variable is distinctly space-separated
add_executable(microwriter
    ${MAIN_SOURCES}
    ${MAIN_HEADERS}
    ${CURSES_SOURCES}
    ${CURSES_HEADERS}
    ${QT6_SOURCES}
    ${QT6_HEADERS}
)

#Expose include directories across module boundaries
targetincludedirectories(microwriter PRIVATE 
    main
    curses 
    qt6 
    ${CURSESINCLUDEDIRS}
)

targetlinklibraries(microwriter PRIVATE 
    Qt6::Widgets 
    Qt6::PrintSupport 
    ${CURSES_LIBRARIES}
)

# Arch Linux 64bit/GCC 16.2.1/CMAKE 4.4.4