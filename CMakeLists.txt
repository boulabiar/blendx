cmake_minimum_required(VERSION 3.20)
project(blendx LANGUAGES C CXX ASM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

set(BLENDX_FONT_FILE "" CACHE FILEPATH "Use a local default font instead of the pinned embedded Roboto font")
if(NOT BLENDX_FONT_FILE)
  set(BLENDX_FONT_FILE "${CMAKE_BINARY_DIR}/assets/Roboto.ttf")
  if(NOT EXISTS "${BLENDX_FONT_FILE}")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/assets")
    file(DOWNLOAD
      "https://raw.githubusercontent.com/google/fonts/ec626514f79f831f1ab848a82114a0ce7e2d6372/ofl/roboto/Roboto%5Bwdth%2Cwght%5D.ttf"
      "${BLENDX_FONT_FILE}"
      EXPECTED_HASH SHA256=d7598e12c5dbef095ff8272cfc55da0250bd07fbdecbac8a530b9b277872a134
      TLS_VERIFY ON
      STATUS BLENDX_FONT_DOWNLOAD_STATUS)
    list(GET BLENDX_FONT_DOWNLOAD_STATUS 0 BLENDX_FONT_DOWNLOAD_CODE)
    if(NOT BLENDX_FONT_DOWNLOAD_CODE EQUAL 0)
      list(GET BLENDX_FONT_DOWNLOAD_STATUS 1 BLENDX_FONT_DOWNLOAD_MESSAGE)
      message(FATAL_ERROR "Could not download pinned Roboto font: ${BLENDX_FONT_DOWNLOAD_MESSAGE}")
    endif()
  endif()
endif()

set(BLENDX_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${BLENDX_GENERATED_DIR}")
set(BLENDX_EMBEDDED_FONT_HEADER "${BLENDX_GENERATED_DIR}/blendx_embedded_font.h")
add_custom_command(
  OUTPUT "${BLENDX_EMBEDDED_FONT_HEADER}"
  COMMAND "${CMAKE_COMMAND}" -DINPUT=${BLENDX_FONT_FILE} -DOUTPUT=${BLENDX_EMBEDDED_FONT_HEADER}
          -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedBinary.cmake"
  DEPENDS "${BLENDX_FONT_FILE}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedBinary.cmake"
  VERBATIM)
add_custom_target(blendx_embedded_assets DEPENDS "${BLENDX_EMBEDDED_FONT_HEADER}")

function(blendx_enable_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra)
  endif()
endfunction()

# SDL2 is normally supplied by the operating system. Prefer its cross-platform
# CMake package and fall back to pkg-config for Linux distributions that only
# ship an SDL2.pc file.
find_package(SDL2 CONFIG QUIET)
if(TARGET SDL2::SDL2)
  set(BLENDX_SDL_TARGET SDL2::SDL2)
elseif(TARGET SDL2::SDL2-static)
  set(BLENDX_SDL_TARGET SDL2::SDL2-static)
else()
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
  set(BLENDX_SDL_TARGET PkgConfig::SDL2)
endif()

set(BLEND2D_ROOT "" CACHE PATH "Use an existing Blend2D source checkout instead of downloading it")
set(BLEND2D_STATIC ON CACHE BOOL "Build Blend2D statically" FORCE)
set(BLEND2D_TEST OFF CACHE BOOL "Disable Blend2D tests" FORCE)
set(BLEND2D_DEMOS OFF CACHE BOOL "Disable Blend2D demos" FORCE)
set(BLEND2D_NO_INSTALL ON CACHE BOOL "Disable Blend2D installation" FORCE)
set(ASMJIT_ROOT "" CACHE PATH "Use an existing AsmJit source checkout instead of downloading it")

if(ASMJIT_ROOT)
  set(ASMJIT_DIR "${ASMJIT_ROOT}" CACHE PATH "Location of AsmJit" FORCE)
elseif(BLEND2D_ROOT AND EXISTS "${BLEND2D_ROOT}/3rdparty/asmjit/CMakeLists.txt")
  # Let Blend2D discover its bundled development checkout.
else()
  FetchContent_Populate(asmjit
    QUIET
    URL https://codeload.github.com/asmjit/asmjit/tar.gz/f64c90818ff2ef87ec4f73f44d0a7e73fbff3229
    URL_HASH SHA256=90d9a1e2b02f2f43d38930fa63a1fd9cded4801b2793a83441c42d6c1277ad87)
  set(ASMJIT_DIR "${asmjit_SOURCE_DIR}" CACHE PATH "Location of AsmJit" FORCE)
endif()

if(BLEND2D_ROOT)
  add_subdirectory("${BLEND2D_ROOT}" "${CMAKE_BINARY_DIR}/blend2d" EXCLUDE_FROM_ALL)
else()
  FetchContent_Declare(blend2d
    URL https://codeload.github.com/blend2d/blend2d/tar.gz/6dbc2cefbc996379e07104e34519a440b49b15d7
    URL_HASH SHA256=212a77ceebf8c37ad5ca7e7eac1757ce7aa911b88085b076a9ed24f6de6e36fa)
  FetchContent_MakeAvailable(blend2d)
endif()

set(YOGA_ROOT "" CACHE PATH "Use an existing Yoga source checkout instead of downloading it")
if(YOGA_ROOT)
  add_subdirectory("${YOGA_ROOT}/yoga" "${CMAKE_BINARY_DIR}/yoga" EXCLUDE_FROM_ALL)
else()
  FetchContent_Populate(yoga
    QUIET
    URL https://codeload.github.com/facebook/yoga/tar.gz/433d4634d11c944bc9e64cfc90927fb10c0525b9
    URL_HASH SHA256=ac2ec0f415422a3ce40c46dede893d730ac84199ca9be2ea4ebe2fce2830a77d)
  # Add only the library. Yoga's top-level project also enables its own tests.
  add_subdirectory("${yoga_SOURCE_DIR}/yoga" "${CMAKE_BINARY_DIR}/yoga" EXCLUDE_FROM_ALL)
endif()

set(HERMES_ROOT "" CACHE PATH "Use an existing Hermes source checkout instead of downloading it")
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
  FetchContent_Declare(hermes
    URL https://codeload.github.com/facebook/hermes/tar.gz/e9edc8b52832df9be82163fc17d2459aa456b0e2
    URL_HASH SHA256=d602adf0120fdc7b6f33b5c0c1c46e5f94ed740d34a52bd5823106a013cb5fce)
  FetchContent_MakeAvailable(hermes)
  set(BLENDX_HERMES_SOURCE_DIR "${hermes_SOURCE_DIR}")
endif()

add_executable(blendx_hermes
  native/hermes_host.cpp
  native/addon.cpp
  native/renderer_model.h
  native/renderer_layout.inc
  native/renderer_paint.inc
  native/napi_protocol.inc
  "${BLENDX_EMBEDDED_FONT_HEADER}")
add_dependencies(blendx_hermes blendx_embedded_assets)
target_compile_definitions(blendx_hermes PRIVATE BLENDX_USE_HERMES_NAPI=1)
target_include_directories(blendx_hermes PRIVATE
  "${BLENDX_GENERATED_DIR}"
  "${BLENDX_HERMES_SOURCE_DIR}/API"
  "${BLENDX_HERMES_SOURCE_DIR}/include"
  "${BLENDX_HERMES_SOURCE_DIR}/public")
target_link_libraries(blendx_hermes PRIVATE
  hermesNapi
  hermesvmlean_a
  yogacore
  blend2d::blend2d
  ${BLENDX_SDL_TARGET})
blendx_enable_warnings(blendx_hermes)
set_target_properties(blendx_hermes PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

option(BLENDX_BUILD_NODE_ADDON "Build the Node-compatible addon used by tests" OFF)
if(BLENDX_BUILD_NODE_ADDON)
  add_library(blendx_native SHARED native/addon.cpp native/renderer_model.h native/renderer_layout.inc
    native/renderer_paint.inc native/napi_protocol.inc
    "${BLENDX_EMBEDDED_FONT_HEADER}")
  add_dependencies(blendx_native blendx_embedded_assets)
  target_include_directories(blendx_native PRIVATE
    "${BLENDX_GENERATED_DIR}"
    "${BLENDX_HERMES_SOURCE_DIR}/API"
    "${BLENDX_HERMES_SOURCE_DIR}/include")
  target_link_libraries(blendx_native PRIVATE yogacore blend2d::blend2d ${BLENDX_SDL_TARGET})
  blendx_enable_warnings(blendx_native)
  set_target_properties(blendx_native PROPERTIES
    PREFIX ""
    SUFFIX ".node"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/dist/native"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/dist/native")
endif()
