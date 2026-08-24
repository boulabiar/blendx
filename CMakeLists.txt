cmake_minimum_required(VERSION 3.20)
project(blendx LANGUAGES C CXX ASM)

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

target_include_directories(blendx_native PRIVATE
  "${NODE_INCLUDE_DIR}")
target_link_libraries(blendx_native PRIVATE
  blend2d::blend2d
  PkgConfig::SDL2)
target_compile_options(blendx_native PRIVATE -Wall -Wextra)

option(BLENDX_ENABLE_HERMES "Build the standalone Hermes runtime" ON)
set(HERMES_ROOT "" CACHE PATH "Optional existing Hermes source checkout")

if(BLENDX_ENABLE_HERMES)
  set(HERMES_ENABLE_TEST_SUITE OFF CACHE BOOL "" FORCE)
  set(HERMES_ENABLE_DEBUGGER OFF CACHE BOOL "" FORCE)
  set(HERMES_ENABLE_INTL OFF CACHE BOOL "" FORCE)
  set(HERMES_UNICODE_LITE ON CACHE BOOL "" FORCE)
  set(HERMES_ENABLE_CORE_EXTENSIONS OFF CACHE BOOL "" FORCE)
  set(HERMES_ENABLE_CONTRIB_EXTENSIONS OFF CACHE BOOL "" FORCE)
  set(HERMES_ENABLE_NAPI ON CACHE BOOL "" FORCE)
  set(HERMES_BUILD_LEAN_LIBHERMES ON CACHE BOOL "" FORCE)
  set(HERMESVM_ALLOW_JIT 0 CACHE STRING "" FORCE)

  if(HERMES_ROOT)
    add_subdirectory("${HERMES_ROOT}" "${CMAKE_BINARY_DIR}/hermes" EXCLUDE_FROM_ALL)
    set(BLENDX_HERMES_SOURCE_DIR "${HERMES_ROOT}")
  else()
    include(FetchContent)
    FetchContent_Declare(hermes
      GIT_REPOSITORY https://github.com/facebook/hermes.git
      GIT_TAG e9edc8b52832df9be82163fc17d2459aa456b0e2
      GIT_SHALLOW FALSE)
    FetchContent_MakeAvailable(hermes)
    set(BLENDX_HERMES_SOURCE_DIR "${hermes_SOURCE_DIR}")
  endif()

  add_executable(blendx_hermes
    native/hermes_host.cpp
    native/addon.cpp)
  target_compile_definitions(blendx_hermes PRIVATE NODE_GYP_MODULE_NAME=blendx_native)
  target_include_directories(blendx_hermes PRIVATE
    "${NODE_INCLUDE_DIR}"
    "${BLENDX_HERMES_SOURCE_DIR}/API"
    "${BLENDX_HERMES_SOURCE_DIR}/include"
    "${BLENDX_HERMES_SOURCE_DIR}/public")
  target_link_libraries(blendx_hermes PRIVATE
    hermesNapi
    hermesvmlean_a
    blend2d::blend2d
    PkgConfig::SDL2)
  target_compile_options(blendx_hermes PRIVATE -Wall -Wextra)
  set_target_properties(blendx_hermes PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif()
