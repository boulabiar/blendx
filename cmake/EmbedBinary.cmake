if(NOT DEFINED INPUT OR NOT DEFINED OUTPUT)
  message(FATAL_ERROR "EmbedBinary.cmake requires INPUT and OUTPUT")
endif()

file(READ "${INPUT}" binary_hex HEX)
string(REGEX REPLACE "([0-9a-fA-F][0-9a-fA-F])" "0x\\1," binary_bytes "${binary_hex}")
file(WRITE "${OUTPUT}"
  "#pragma once\n#include <cstddef>\n"
  "inline constexpr unsigned char kBlendxEmbeddedFont[] = {${binary_bytes}};\n"
  "inline constexpr std::size_t kBlendxEmbeddedFontSize = sizeof(kBlendxEmbeddedFont);\n")
