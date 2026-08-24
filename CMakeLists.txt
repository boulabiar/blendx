cmake_minimum_required(VERSION 3.20)
project(blendx_native LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

set(BLEND2D_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../blend2d" CACHE PATH "Blend2D source directory")
set(BLEND2D_LIBRARY "${BLEND2D_ROOT}/build/libblend2d.so" CACHE FILEPATH "Blend2D shared library")
find_path(NODE_INCLUDE_DIR node_api.h
  PATHS
    /usr/include/node
    "$ENV{NVM_BIN}/../include/node"
  REQUIRED)

add_library(blendx_native SHARED native/addon.cpp)
set_target_properties(blendx_native PROPERTIES
  PREFIX ""
  SUFFIX ".node"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/dist/native")

target_include_directories(blendx_native PRIVATE
  "${BLEND2D_ROOT}"
  "${NODE_INCLUDE_DIR}")
target_link_libraries(blendx_native PRIVATE
  "${BLEND2D_LIBRARY}"
  PkgConfig::SDL2)
target_compile_options(blendx_native PRIVATE -Wall -Wextra)

# Make the addon runnable without installing the sibling Blend2D checkout.
set_target_properties(blendx_native PROPERTIES
  BUILD_RPATH "${BLEND2D_ROOT}/build"
  INSTALL_RPATH "${BLEND2D_ROOT}/build")
