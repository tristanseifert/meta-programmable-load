####################################################################################################
# Load interface daemon
#
# It handles loading firmware to the coprocessor, and dealing with the exposed communications
# interfaces.
####################################################################################################
cmake_minimum_required(VERSION 3.18 FATAL_ERROR)
project(load-loadd VERSION 1.0 LANGUAGES C CXX)

include(ExternalProject)
include(FetchContent)

###############
# Set warning levels and language version
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wmissing-declarations -Wformat=2 -fdiagnostics-color=always -Wundef
    -Wcast-qual -Wwrite-strings -Wno-format-nonliteral)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options(-Werror -Wimplicit-fallthrough -Wno-deprecated-copy -Wno-address-of-packed-member
        -Wno-expansion-to-defined -Wno-undef -Wno-unused-private-field -Wno-deprecated-volatile)
endif()

###############
# Find our dependencies
find_package(PkgConfig REQUIRED)

find_package(SQLite3 REQUIRED)

pkg_search_module(PKG_LIBCBOR REQUIRED libcbor)
link_directories(${PKG_LIBCBOR_LIBRARY_DIRS})

pkg_search_module(PKG_LIBEVENT REQUIRED libevent)
link_directories(${PKG_LIBEVENT_LIBRARY_DIRS})

# Remote dependencies
#add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/tomlplusplus EXCLUDE_FROM_ALL)

find_package(fmt REQUIRED)
find_package(plog REQUIRED)

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
# Load daemon
#
# Main executable for the daemon
add_executable(daemon
    ${VERSION_FILE}
    src/main.cpp
    src/Config.cpp
    src/Watchdog.cpp
    src/Coprocessor.cpp
    src/ConfdEpHandler.cpp
    src/ControlEpHandler.cpp
    src/RpcServer.cpp
)

set_target_properties(daemon PROPERTIES OUTPUT_NAME loadd)

target_include_directories(daemon PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)
target_include_directories(daemon PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include/daemon)
#target_link_libraries(daemon PRIVATE SQLite::SQLite3 plog::plog fmt::fmt
#    tomlplusplus::tomlplusplus)
target_link_libraries(daemon PRIVATE plog::plog fmt::fmt)

target_include_directories(daemon PRIVATE ${PKG_LIBEVENT_INCLUDE_DIRS} ${PKG_LIBCBOR_INCLUDE_DIRS})
target_link_libraries(daemon PRIVATE ${PKG_LIBEVENT_LIBRARIES} ${PKG_LIBCBOR_LIBRARIES})

INSTALL(TARGETS daemon RUNTIME DESTINATION /usr/sbin)

# add systemd support on linux
if(UNIX AND NOT APPLE)
    pkg_search_module(PKG_SYSTEMD libsystemd)

    target_include_directories(daemon PRIVATE ${PKG_SYSTEMD_INCLUDE_DIRS})
    target_link_libraries(daemon PRIVATE ${PKG_SYSTEMD_LIBRARIES})
endif()

