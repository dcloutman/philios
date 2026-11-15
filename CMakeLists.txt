cmake_minimum_required(VERSION 3.28)
project(philios LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(PHILIOS_CLANG_TIDY "Enable clang-tidy during builds" OFF)
option(PHILIOS_SANITIZE "Enable ASan+UBSan" OFF)

include(CTest)

# Directory-level options apply only to targets defined after this point.
if(PHILIOS_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
endif()

file(GLOB_RECURSE PHILIOS_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
list(FILTER PHILIOS_CORE_SOURCES EXCLUDE REGEX "/src/main\.cpp$")

add_library(philios_core ${PHILIOS_CORE_SOURCES})
target_include_directories(philios_core PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_options(philios_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)

add_executable(philios src/main.cpp)
target_link_libraries(philios PRIVATE philios_core)
target_include_directories(philios PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_options(philios PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)

if(PHILIOS_CLANG_TIDY)
  find_program(CLANG_TIDY_EXE NAMES clang-tidy REQUIRED)
  set_target_properties(philios_core PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
  set_target_properties(philios PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
endif()

if(BUILD_TESTING)
  find_package(Catch2 3.16.1 EXACT REQUIRED)
  file(GLOB_RECURSE PHILIOS_TEST_SOURCES CONFIGURE_DEPENDS
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/*.cpp")

  add_executable(philios_tests ${PHILIOS_TEST_SOURCES})
  target_link_libraries(philios_tests PRIVATE philios_core Catch2::Catch2WithMain)
  target_include_directories(philios_tests PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/src"
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit")
  target_compile_options(philios_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)

  if(PHILIOS_CLANG_TIDY)
    set_target_properties(philios_tests PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
  endif()

  add_test(NAME philios_tests COMMAND philios_tests)
endif()
