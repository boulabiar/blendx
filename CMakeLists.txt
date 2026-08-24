cmake_minimum_required(VERSION 3.20)
project(blendx_native LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)

set(BLEND2D_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../blend2d" CACHE PATH "Blend2D source directory")
set(BLEND2D_STATIC ON CACHE BOOL "Build Blend2D statically" FORCE)
set(BLEND2D_TEST OFF CACHE BOOL "Disable Blend2D tests" FORCE)
set(BLEND2D_DEMOS OFF CACHE BOOL "Disable Blend2D demos" FORCE)
set(BLEND2D_NO_INSTALL ON CACHE BOOL "Disable Blend2D installation" FORCE)
add_subdirectory("${BLEND2D_ROOT}" "${CMAKE_BINARY_DIR}/blend2d" EXCLUDE_FROM_ALL)
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

# Bun compiles the TypeScript sources directly and recognizes a statically
# imported .node file. Keep a source-tree copy for `bun build --compile`, while
# retaining dist/native for declaration builds and compatibility with dist/.
add_custom_command(TARGET blendx_native POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
          "$<TARGET_FILE:blendx_native>"
          "${CMAKE_CURRENT_SOURCE_DIR}/native/blendx_native.node")

target_include_directories(blendx_native PRIVATE
  "${NODE_INCLUDE_DIR}")
target_link_libraries(blendx_native PRIVATE
  blend2d::blend2d
  PkgConfig::SDL2)
target_compile_options(blendx_native PRIVATE -Wall -Wextra)
