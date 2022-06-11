####################################################################################################
# Config daemon
#
# This is a simple key/value store, which stores the underlying data in an sqlite3 database. It is
# accessible to other apps via a local RPC interface over domain sockets. A helper library is
# provided to handle the RPC interface.
####################################################################################################
cmake_minimum_required(VERSION 3.18 FATAL_ERROR)
project(load-confd VERSION 1.0 LANGUAGES C CXX)

###############
# Set warning levels and language version
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wmissing-declarations -Wformat=2 -fdiagnostics-color=always
    -ftls-model=initial-exec -Wundef -Wcast-qual -Wwrite-strings)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options(-Werror -Wimplicit-fallthrough -Wno-deprecated-copy -Wno-address-of-packed-member
        -Wno-expansion-to-defined -Wno-undef -Wno-unused-private-field -Wno-deprecated-volatile)
endif()

###############
# Find our dependencies
find_package(PkgConfig)

find_package(SQLite3 REQUIRED)
pkg_search_module(PKG_LIBCBOR libcbor)

###############
# Config daemon target
#
# This is the binary of the daemon that will serve configuration requests. Installed alongside it
# are systemd unit files.
add_executable(daemon
    src/daemon/main.cpp
    src/daemon/watchdog.cpp
)
set_target_properties(daemon PROPERTIES OUTPUT_NAME confd)

target_include_directories(daemon PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)
target_include_directories(daemon PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include/daemon)
target_link_libraries(daemon PRIVATE SQLite::SQLite3 rpc-helper)

INSTALL(TARGETS daemon RUNTIME DESTINATION /usr/sbin)

# add systemd support on linux
if(UNIX AND NOT APPLE)
    pkg_search_module(PKG_SYSTEMD libsystemd)

    target_include_directories(daemon PRIVATE ${PKG_SYSTEMD_INCLUDE_DIRS})
    target_link_libraries(daemon PRIVATE ${PKG_SYSTEMD_LIBRARIES})
endif()

###############
# RPC helper library
#
# Contains stubs for serializing and deserializing messages; consumed internally by the daemon and
# the shared config query library.
add_library(rpc-helper STATIC
    src/rpc/deserialize.cpp
    src/rpc/serialize.cpp
)

target_include_directories(rpc-helper PRIVATE ${PKG_LIBCBOR_INCLUDE_DIRS})
target_link_libraries(rpc-helper PRIVATE ${PKG_LIBCBOR_LIBRARIES})
target_include_directories(rpc-helper PRIVATE src/rpc)
target_include_directories(rpc-helper PUBLIC include/rpc)

###############
# User library
#
# Provides an interface to query and update the configuration from applications with a simple C-
# style interface.

