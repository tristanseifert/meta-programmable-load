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

# find pango using pkgconfig
pkg_search_module(PKG_PANGO pango)
pkg_search_module(PKG_PANGOCAIRO pangocairo)

# glib is a dependency of harfbuzz and pango
pkg_search_module(PKG_GLIB2 glib-2.0)
pkg_search_module(PKG_GOBJECT2 gobject-2.0)

###############
# Splash daemon target
#
# Builds the splash daemon, started early during boot, which opens a fixed framebuffer device and
# renders the initial screen.
add_executable(splash
    src/splash/main.cpp
    src/splash/Drawer.cpp
    src/splash/RpcListener.cpp
)
set_target_properties(splash PROPERTIES OUTPUT_NAME bootsplash)

target_include_directories(splash PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)
target_include_directories(splash PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include/splash)

target_include_directories(splash PRIVATE ${PKG_PANGO_INCLUDE_DIRS})
target_link_libraries(splash PRIVATE ${LIB_PNG} ${LIB_FREETYPE} ${LIB_CAIRO} ${LIB_HARFBUZZ}
    ${PKG_PANGO_LIBRARIES} ${PKG_GLIB2_LIBRARIES} ${PKG_GOBJECT2_LIBRARIES} ${PKG_PANGOCAIRO_LIBRARIES})

INSTALL(TARGETS splash RUNTIME DESTINATION /usr/sbin)

###############
# Splash remote library
#
# This is a dynamic library that implements the RPC protocol between the splash daemon and any
# applications that wish to control it.
add_library(remotelib SHARED
    src/remotelib/glue.cpp
)

target_include_directories(remotelib PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)
target_include_directories(remotelib PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include/remotelib)

set_target_properties(remotelib PROPERTIES LIBRARY_OUTPUT_NAME splash-remote)
set_target_properties(remotelib PROPERTIES PUBLIC_HEADER ${CMAKE_CURRENT_LIST_DIR}/include/remotelib/remote.h)
set_target_properties(remotelib PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(remotelib PROPERTIES SOVERSION 1)

INSTALL(TARGETS remotelib LIBRARY
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/splash)

###############
# Splash control
#
# A command line utility to communicate with the splash daemon and update the messages and
# progress information on the screen; it also allows requesting termination of the daemon once
# boot has completed.
add_executable(splashctl
    src/remotectl/main.cpp
)
set_target_properties(splashctl PROPERTIES OUTPUT_NAME bootsplashctl)
target_link_libraries(splashctl PRIVATE remotelib)

INSTALL(TARGETS splashctl RUNTIME DESTINATION /usr/bin)
