cmake_minimum_required(VERSION 3.16)
project(ai_meta
  VERSION 0.1.0
  DESCRIPTION "Detect, extract, strip, and write AI-generation metadata in images"
  HOMEPAGE_URL "https://github.com/localtools/ai-meta"
  LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(ai_meta
  src/ai_meta.c
  src/util.c
  src/png_meta.c
  src/jpeg_meta.c
  src/webp_meta.c
  src/exif_basic.c
)
target_include_directories(ai_meta PUBLIC include PRIVATE src)
find_package(ZLIB REQUIRED)
target_link_libraries(ai_meta PUBLIC ZLIB::ZLIB)

add_executable(ai_meta_cli tools/ai_meta_cli.c)
target_link_libraries(ai_meta_cli PRIVATE ai_meta)
set_target_properties(ai_meta_cli PROPERTIES OUTPUT_NAME ai_meta)

enable_testing()
add_executable(test_ai_meta tests/test_ai_meta.c)
target_link_libraries(test_ai_meta PRIVATE ai_meta)
add_test(NAME ai_meta_tests COMMAND test_ai_meta WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
