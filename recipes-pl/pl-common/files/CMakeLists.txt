####################################################################################################
# Programmable load shared code
#
# This builds a static library, which may in turn be linked into programmable load software. It
# provides various common code, such as logging setup, a libevent-based event loop, and a generic
# RPC server.
####################################################################################################
cmake_minimum_required(VERSION 3.18 FATAL_ERROR)
project(load-common VERSION 1.0 LANGUAGES C CXX)

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

pkg_search_module(PKG_LIBCBOR REQUIRED libcbor)
link_directories(${PKG_LIBCBOR_LIBRARY_DIRS})

pkg_search_module(PKG_LIBEVENT REQUIRED libevent)
link_directories(${PKG_LIBEVENT_LIBRARY_DIRS})

find_package(fmt REQUIRED)
find_package(plog REQUIRED)

###############
add_library(${PROJECT_NAME} STATIC
    src/EventLoop.cpp
    src/Watchdog.cpp
    src/Logging.cpp
    src/Rpc/ClientBase.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE src ${CMAKE_CURRENT_LIST_DIR}/include/)
target_link_libraries(${PROJECT_NAME} PUBLIC plog::plog fmt::fmt)

target_include_directories(${PROJECT_NAME} PRIVATE ${PKG_LIBEVENT_INCLUDE_DIRS} ${PKG_LIBCBOR_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PUBLIC ${PKG_LIBEVENT_LIBRARIES} ${PKG_LIBCBOR_LIBRARIES})

# add systemd support on linux
if(UNIX AND NOT APPLE)
    pkg_search_module(PKG_SYSTEMD libsystemd)
    link_directories(${PKG_SYSTEMD_LIBRARY_DIRS})

    target_include_directories(${PROJECT_NAME} PRIVATE ${PKG_SYSTEMD_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} PUBLIC ${PKG_SYSTEMD_LIBRARIES})
endif()

# install phase
include(GNUInstallDirs)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

install(
    TARGETS ${PROJECT_NAME}
    ARCHIVE
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(
    TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}Config
)

install(
    EXPORT ${PROJECT_NAME}Config
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
    NAMESPACE ${PROJECT_NAME}::
)

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/${PROJECT_NAME}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
    COMPATIBILITY AnyNewerVersion
)

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)
