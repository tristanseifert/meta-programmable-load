####################################################################################################
# Pinball
#
# User-mode driver for all front panel hardware (LED controllers, IO expanders, display
# controllers and touch drivers) as well as some auxiliary on-board hardware to do with the user
# interface (such as beepers, and LEDs managed by hardware GPIOs.) It exposes a basic RPC interface
# to other UI tasks.
####################################################################################################
cmake_minimum_required(VERSION 3.18 FATAL_ERROR)
project(load-pinballd VERSION 1.0 LANGUAGES C CXX)

include(ExternalProject)
include(FetchContent)

###############
# Set warning levels and language version
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wmissing-declarations -Wformat=2 -fdiagnostics-color=always
    -ftls-model=initial-exec -Wundef -Wcast-qual -Wwrite-strings -Wno-format-nonliteral)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options(-Werror -Wimplicit-fallthrough -Wno-deprecated-copy -Wno-address-of-packed-member
        -Wno-expansion-to-defined -Wno-undef -Wno-unused-private-field -Wno-deprecated-volatile)
endif()

###############
# Find our dependencies
find_package(PkgConfig REQUIRED)
find_package(Git REQUIRED)

pkg_search_module(PKG_LIBCBOR REQUIRED libcbor)
link_directories(${PKG_LIBCBOR_LIBRARY_DIRS})

pkg_search_module(PKG_LIBEVENT REQUIRED libevent)
link_directories(${PKG_LIBEVENT_LIBRARY_DIRS})

pkg_search_module(PKG_GPIOD REQUIRED libgpiod)
link_directories(${PKG_GPIOD_LIBRARY_DIRS})

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/plog EXCLUDE_FROM_ALL)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/fmt EXCLUDE_FROM_ALL)

###############
# Create version file
find_package(Git REQUIRED)
execute_process(
    COMMAND ${GIT_EXECUTABLE} log -1 --format=%h
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    OUTPUT_VARIABLE GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(VERSION_FILE ${CMAKE_CURRENT_BINARY_DIR}/version.c)
configure_file(${CMAKE_CURRENT_LIST_DIR}/src/version.c.in ${VERSION_FILE} @ONLY)

###############
# Pinball daemon target
#
# The actual UI driver daemon
add_executable(daemon
    src/daemon/main.cpp
    src/daemon/Probulator.cpp
    src/daemon/Watchdog.cpp
    src/daemon/drivers/gpio/Pca9535.cpp
    src/daemon/drivers/lcd/Nt35510.cpp
    ${VERSION_FILE}
)
set_target_properties(daemon PROPERTIES OUTPUT_NAME pinballd)
target_include_directories(daemon PRIVATE src/daemon ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(daemon PRIVATE plog::plog fmt::fmt)

target_include_directories(daemon PRIVATE ${PKG_LIBEVENT_INCLUDE_DIRS} ${PKG_LIBCBOR_INCLUDE_DIRS} ${PKG_GPIOD_INCLUDE_DIRS})
target_link_libraries(daemon PRIVATE ${PKG_LIBEVENT_LIBRARIES} ${PKG_LIBCBOR_LIBRARIES} ${PKG_GPIOD_LIBRARIES})

# add systemd support on linux
if(UNIX AND NOT APPLE)
    pkg_search_module(PKG_SYSTEMD libsystemd)

    target_include_directories(daemon PRIVATE ${PKG_SYSTEMD_INCLUDE_DIRS})
    target_link_libraries(daemon PRIVATE ${PKG_SYSTEMD_LIBRARIES})
endif()

INSTALL(TARGETS daemon RUNTIME DESTINATION /usr/sbin)

