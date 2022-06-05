####################################################################################################
# Framebuffer splash
#
# Draws an informational text about the boot progress, version info, etc. on the screen as the
# machine boots. It is controlled via a separate control program, which communicates with the
# splash daemon via a local network socket.
#
# Note that all configuration is fixed in the binary at compile-time.
####################################################################################################
cmake_minimum_required(VERSION 3.18 FATAL_ERROR)
project(boot-splash VERSION 1.0 LANGUAGES C CXX)

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
find_package(PkgConfig)

# Find our dependencies
find_library(LIB_PNG png REQUIRED PATHS /usr/lib)
find_library(LIB_FREETYPE freetype REQUIRED)
find_library(LIB_CAIRO cairo REQUIRED)
find_library(LIB_HARFBUZZ harfbuzz REQUIRED)

# pango uses pkgconfig
pkg_search_module(PKG_PANGO pango)
find_library(LIB_PANGO pango-1.0 HINTS ${PKG_PANGO_LIBRARY_DIRS} REQUIRED)

###############
# Splash daemon target
#
# Builds the splash daemon, started early during boot, which opens a fixed framebuffer device and
# renders the initial screen.
add_executable(boot-splash
    src/splash/main.cpp
    src/splash/Drawer.cpp
)

target_include_directories(boot-splash PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include/splash)
target_link_libraries(boot-splash PRIVATE ${LIB_PNG} ${LIB_FREETYPE} ${LIB_CAIRO} ${LIB_HARFBUZZ}
    ${LIB_PANGO})

INSTALL(TARGETS boot-splash RUNTIME DESTINATION /usr/sbin)

###############
# Splash control
#
# A command line utility to communicate with the splash daemon and update the messages and
# progress information on the screen; it also allows requesting termination of the daemon once
# boot has completed.

# TODO: implement
