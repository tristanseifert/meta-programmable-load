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
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/plog EXCLUDE_FROM_ALL)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/fmt EXCLUDE_FROM_ALL)

set(LV_CONF_PATH ${CMAKE_CURRENT_LIST_DIR}/include/lvgl/lv_conf.h CACHE FILEPATH "" FORCE)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/lvgl EXCLUDE_FROM_ALL)

target_link_libraries(lvgl PUBLIC ${PKG_FREETYPE_LIBRARIES})

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
    src/EventLoop.cpp
    src/Framebuffer.cpp
    src/Gui.cpp
    src/Watchdog.cpp
    src/Gui/FontHandler.cpp
)

set_target_properties(gui PROPERTIES OUTPUT_NAME load-gui)

target_include_directories(gui PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
target_include_directories(gui PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)
target_include_directories(gui PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include/gui)
target_link_libraries(gui PRIVATE confd plog::plog fmt::fmt)
#    tomlplusplus::tomlplusplus)

target_include_directories(gui PRIVATE ${PKG_LIBEVENT_INCLUDE_DIRS} ${PKG_LIBCBOR_INCLUDE_DIRS})
target_link_libraries(gui PRIVATE ${PKG_LIBEVENT_LIBRARIES} ${PKG_LIBCBOR_LIBRARIES})
# gui libraries
target_link_libraries(gui PRIVATE ${PKG_PNG_LIBRARIES} ${PKG_FREETYPE_LIBRARIES}
    ${PKG_LIBDRM_LIBRARIES} ${PKG_LIBKMS_LIBRARIES} ${PKG_FONTCONFIG_LIBRARIES} lvgl::lvgl)
target_link_options(gui PRIVATE ${PKG_FREETYPE_LDFLAGS})


INSTALL(TARGETS gui RUNTIME DESTINATION /usr/sbin)

# add systemd support on linux
if(UNIX AND NOT APPLE)
    pkg_search_module(PKG_SYSTEMD libsystemd)

    target_include_directories(gui PRIVATE ${PKG_SYSTEMD_INCLUDE_DIRS})
    target_link_libraries(gui PRIVATE ${PKG_SYSTEMD_LIBRARIES})
endif()

###############
# LVGL C++ wrapper
add_library(lvglpp STATIC
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/core/display.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/core/event.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/core/indev.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/core/object.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/core/group.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/core/theme.cpp

    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/draw/desc.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/draw/image.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/draw/mask.cpp

    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/font/font.cpp

    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/misc/anim.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/misc/area.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/misc/color.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/misc/fs.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/misc/style.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/misc/timer.cpp

    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/widgets/animimg.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/widgets/arc.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/widgets/bar.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/widgets/btnmatrix.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/widgets/calendar.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/widgets/canvas.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/widgets/chart.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/widgets/checkbox.cpp
    # does not currently compile
    #    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/widgets/colorwheel.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/widgets/dropdown.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/widgets/image.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/widgets/imgbtn.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/widgets/keyboard.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/widgets/label.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/widgets/led.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/widgets/line.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/widgets/list.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/widgets/menu.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/widgets/meter.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/widgets/msgbox.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/widgets/roller.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/widgets/slider.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/widgets/span.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/widgets/spinbox.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/widgets/spinner.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/widgets/table.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/widgets/tabview.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/widgets/textarea.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/widgets/tileview.cpp
    ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src/lvglpp/widgets/win.cpp
)
target_compile_options(lvglpp PUBLIC "-Wno-unused-function")
target_include_directories(lvglpp PUBLIC ${CMAKE_CURRENT_LIST_DIR}/lib/lvglpp/src)
target_link_libraries(lvglpp PUBLIC lvgl::lvgl)

target_link_libraries(gui PRIVATE lvglpp)
