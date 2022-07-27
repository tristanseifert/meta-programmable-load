####################################################################################################
# Load user interface
#
# Graphical user interface for the load. This communicates with the front panel controller and
# touch screen via I²C (as in-process drivers) as well as with the embedded load hardware via the
# loadd rpc interface.
####################################################################################################
cmake_minimum_required(VERSION 3.18 FATAL_ERROR)
project(load-gui VERSION 0.1 LANGUAGES C CXX)

###############
# Set warning levels and language version
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wmissing-declarations -Wformat=2 -fdiagnostics-color=always -Wundef
    -Wwrite-strings -Wno-format-nonliteral)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options(-Werror -Wimplicit-fallthrough -Wno-deprecated-copy -Wno-address-of-packed-member
        -Wno-expansion-to-defined -Wno-undef -Wno-unused-private-field -Wno-deprecated-volatile)
endif()

###############
# Find our dependencies
find_package(PkgConfig REQUIRED)

pkg_search_module(PKG_LIBCBOR REQUIRED libcbor)
link_directories(${PKG_LIBCBOR_LIBRARY_DIRS})
include_directories(${PKG_LIBCBOR_INCLUDE_DIRS})

pkg_search_module(PKG_LIBEVENT REQUIRED libevent)
link_directories(${PKG_LIBEVENT_LIBRARY_DIRS})
include_directories(${PKG_LIBEVENT_INCLUDE_DIRS})

pkg_search_module(PKG_PNG REQUIRED libpng)
link_directories(${PKG_PNG_LIBRARY_DIRS})
include_directories(${PKG_PNG_INCLUDE_DIRS})

pkg_search_module(PKG_FREETYPE REQUIRED freetype2)
link_directories(${PKG_FREETYPE_LIBRARY_DIRS})
include_directories(${PKG_FREETYPE_INCLUDE_DIRS})

pkg_search_module(PKG_FONTCONFIG REQUIRED fontconfig)
link_directories(${PKG_FONTCONFIG_LIBRARY_DIRS})
include_directories(${PKG_FONTCONFIG_INCLUDE_DIRS})

pkg_search_module(PKG_LIBDRM REQUIRED libdrm)
link_directories(${PKG_LIBDRM_LIBRARY_DIRS})
include_directories(${PKG_LIBDRM_INCLUDE_DIRS})

pkg_search_module(PKG_LIBKMS REQUIRED libkms)
link_directories(${PKG_LIBKMS_LIBRARY_DIRS})
include_directories(${PKG_LIBKMS_INCLUDE_DIRS})

# Remote dependencies (as submodules)
#add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/tomlplusplus EXCLUDE_FROM_ALL)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/shittygui EXCLUDE_FROM_ALL)

find_package(load-common REQUIRED)
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
# add more warning flags for our own code
add_compile_options(-Wcast-qual)

###############
# User interface app
add_executable(gui
    ${VERSION_FILE}
    src/main.cpp
    src/Framebuffer.cpp
    src/Rpc/PinballClient.cpp
    src/Rpc/LoaddClient.cpp
    src/Gui/IconManager.cpp
    src/Gui/Renderer.cpp
    src/Gui/HomeScreen.cpp
    src/Gui/Setup/AuxOut.cpp
)

set_target_properties(gui PROPERTIES OUTPUT_NAME load-gui)

target_include_directories(gui PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
target_include_directories(gui PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)
target_include_directories(gui PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include/gui)
target_link_libraries(gui PRIVATE confd plog::plog fmt::fmt load-common::load-common)
#    tomlplusplus::tomlplusplus)

target_include_directories(gui PRIVATE ${PKG_LIBEVENT_INCLUDE_DIRS} ${PKG_LIBCBOR_INCLUDE_DIRS})
target_link_libraries(gui PRIVATE ${PKG_LIBEVENT_LIBRARIES} ${PKG_LIBCBOR_LIBRARIES})
# gui libraries
target_link_libraries(gui PRIVATE shittygui::shittygui ${PKG_PNG_LIBRARIES} ${PKG_FREETYPE_LIBRARIES}
    ${PKG_LIBDRM_LIBRARIES} ${PKG_LIBKMS_LIBRARIES} ${PKG_FONTCONFIG_LIBRARIES})
target_link_options(gui PRIVATE ${PKG_FREETYPE_LDFLAGS})


INSTALL(TARGETS gui RUNTIME DESTINATION /usr/sbin)

# copy icons
file(COPY ${CMAKE_CURRENT_LIST_DIR}/data/icons/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/icons)

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/icons
    DESTINATION /usr/share/pl-gui
    PATTERN "README.md" EXCLUDE)

# add systemd support on linux
if(UNIX AND NOT APPLE)
    pkg_search_module(PKG_SYSTEMD libsystemd)

    target_include_directories(gui PRIVATE ${PKG_SYSTEMD_INCLUDE_DIRS})
    target_link_libraries(gui PRIVATE ${PKG_SYSTEMD_LIBRARIES})
endif()

