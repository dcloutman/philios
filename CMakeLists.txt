cmake_minimum_required(VERSION 3.16)
project(my-lang VERSION 0.1.0 LANGUAGES CXX)

# Enforce strict C++20 standard across your project and Catch2
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 1. Core interpreter library and CLI executable
add_library(interpreter_core src/lexer.cpp src/parser.cpp)
target_include_directories(interpreter_core PUBLIC include)

add_executable(my-lang src/main.cpp)
target_link_libraries(my-lang PRIVATE interpreter_core)

# 2. Enable 'make test' with Catch2
include(CTest)
if(BUILD_TESTING)
  include(FetchContent)
  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com
    GIT_TAG        v3.7.1
  )
  FetchContent_MakeAvailable(Catch2)

  add_executable(unit_tests tests/test_lexer.cpp)
  target_link_libraries(unit_tests PRIVATE interpreter_core Catch2::Catch2WithMain)

  include(Catch)
  catch_discover_tests(unit_tests)
endif()

# 3. Enable 'sudo make install'
include(GNUInstallDirs)
install(TARGETS my-lang
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

