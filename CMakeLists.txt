cmake_minimum_required(VERSION 3.28)
project(philios LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(PHILIOS_CLANG_TIDY "Run clang-tidy (cert-*, bugprone-*) on every project target" ON)
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

add_library(philios_core STATIC ${PHILIOS_CORE_SOURCES})
target_include_directories(philios_core PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_options(philios_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)

add_executable(philios src/main.cpp)
target_link_libraries(philios PRIVATE philios_core)
target_include_directories(philios PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_options(philios PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)

if(PHILIOS_CLANG_TIDY)
  find_program(CLANG_TIDY_EXE NAMES clang-tidy)
  if(NOT CLANG_TIDY_EXE)
    message(FATAL_ERROR
        "PHILIOS_CLANG_TIDY is ON but clang-tidy was not found. Install clang-tidy 18 or later "
        "(for example: sudo apt install clang-tidy), or configure with -DPHILIOS_CLANG_TIDY=OFF "
        "for fast local iteration only; verification and acceptance require it ON.")
  endif()
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

  set(PHILIOS_TCLI10_TEST_CASE "T-CLI-10 terminal device main.phil")

  add_test(NAME philios_tests COMMAND philios_tests "~${PHILIOS_TCLI10_TEST_CASE}")

  add_test(NAME t_cli_10_terminal_device_main_phil COMMAND philios_tests "${PHILIOS_TCLI10_TEST_CASE}")
  set_tests_properties(t_cli_10_terminal_device_main_phil PROPERTIES
      SKIP_REGULAR_EXPRESSION "T-CLI-10 SKIPPED: .+"
      TIMEOUT 10)

  add_test(NAME t_build_6_posix_confinement
      COMMAND ${CMAKE_COMMAND}
              -DPHILIOS_SOURCE_DIR=${CMAKE_SOURCE_DIR}
              -P ${CMAKE_SOURCE_DIR}/tests/build/check_posix_confinement.cmake)

  add_test(NAME t_build_7_tcli10_filter_guard
      COMMAND philios_tests "__philios_tcli10_nonexistent_case__")
  set_tests_properties(t_build_7_tcli10_filter_guard PROPERTIES WILL_FAIL TRUE)

  # Build probes T-BUILD-1 to T-BUILD-5 (§7, §9). Target facts are written at generate time so the
  # probes read the configured values rather than restating them.
  set(philios_build_facts "")
  foreach(fact_target IN ITEMS philios_core philios philios_tests)
    foreach(fact_property IN ITEMS TYPE CXX_CLANG_TIDY SOURCES INCLUDE_DIRECTORIES
                                   LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
      string(APPEND philios_build_facts
          "set(FACT_${fact_target}_${fact_property} [==[$<TARGET_PROPERTY:${fact_target},${fact_property}>]==])\n")
    endforeach()
  endforeach()
  file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/philios_build_facts.cmake"
      CONTENT "${philios_build_facts}")
  set(philios_build_probe_args
      -DPHILIOS_SOURCE_DIR=${CMAKE_SOURCE_DIR}
      -DPHILIOS_BINARY_DIR=${CMAKE_BINARY_DIR}
      "-DPHILIOS_GENERATOR=${CMAKE_GENERATOR}")

  if(PHILIOS_CLANG_TIDY)
    add_test(NAME t_build_1_clang_tidy_all_targets
        COMMAND ${CMAKE_COMMAND} ${philios_build_probe_args} -DCLANG_TIDY_EXE=${CLANG_TIDY_EXE}
                -P ${CMAKE_SOURCE_DIR}/tests/build/check_tidy_enabled.cmake)
    add_test(NAME t_build_5_clang_tidy_scope
        COMMAND ${CMAKE_COMMAND} ${philios_build_probe_args} -DCLANG_TIDY_EXE=${CLANG_TIDY_EXE}
                -P ${CMAKE_SOURCE_DIR}/tests/build/check_tidy_scope.cmake)
    set_tests_properties(t_build_5_clang_tidy_scope PROPERTIES TIMEOUT 300)
  endif()

  add_test(NAME t_build_2_missing_clang_tidy
      COMMAND ${CMAKE_COMMAND} ${philios_build_probe_args}
              -DPHILIOS_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
              -DPHILIOS_CXX_COMPILER=${CMAKE_CXX_COMPILER}
              -P ${CMAKE_SOURCE_DIR}/tests/build/check_tidy_missing.cmake)
  set_tests_properties(t_build_2_missing_clang_tidy PROPERTIES TIMEOUT 300)

  add_test(NAME t_build_3_strict_cxx20
      COMMAND ${CMAKE_COMMAND} ${philios_build_probe_args} -DCHECK=std
              -P ${CMAKE_SOURCE_DIR}/tests/build/check_compile_database.cmake)
  add_test(NAME t_build_4_test_separation
      COMMAND ${CMAKE_COMMAND} ${philios_build_probe_args} -DCHECK=separation
              -P ${CMAKE_SOURCE_DIR}/tests/build/check_compile_database.cmake)

  # CLI golden tests (§7, §9): run the real `philios` binary through tests/cli/run_case.cmake,
  # which prepares main.phil (a committed fixture from tests/cli/cases/, or an input generated at
  # test time), then checks the exit status, the stderr text (class/line/column prefix, usage,
  # startup, or empty), and an empty stdout. Keywords map to the driver variables documented in
  # run_case.cmake:
  #   philios_cli_test(<name> [MODE <mode>] [FIXTURE <file>] [ARGS <arg>...] [STATUS <n>]
  #                    [KIND empty|located|startup|usage] [CLASS <c>] [LINE <l>] [COLUMN <c>]
  #                    [MESSAGE <text>] [FORBID <text>] [SIZE <n>] [EXIT_VALUE <n>] [NESTING <n>]
  #                    [DEPTH <n>] [SKIP_LABEL <label>] [NO_CRASH])
  function(philios_cli_test name)
    cmake_parse_arguments(PARSE_ARGV 1 CASE "NO_CRASH"
        "MODE;FIXTURE;STATUS;KIND;CLASS;LINE;COLUMN;MESSAGE;FORBID;SIZE;EXIT_VALUE;NESTING;DEPTH;SKIP_LABEL"
        "ARGS")
    if(CASE_UNPARSED_ARGUMENTS)
      message(FATAL_ERROR "philios_cli_test(${name}): unknown arguments ${CASE_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT CASE_MODE)
      set(CASE_MODE file)
    endif()
    set(case_args
        -DPHILIOS_BIN=$<TARGET_FILE:philios>
        -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/cli_cases/${name}
        -DMODE=${CASE_MODE})
    if(CASE_FIXTURE)
      list(APPEND case_args -DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/tests/cli/cases/${CASE_FIXTURE})
    endif()
    if(CASE_ARGS)
      string(REPLACE ";" "\\;" escaped_args "${CASE_ARGS}")
      list(APPEND case_args "-DARGS=${escaped_args}")
    endif()
    if(CASE_NO_CRASH)
      list(APPEND case_args -DEXPECT_NO_CRASH=TRUE)
    else()
      list(APPEND case_args -DEXPECT_STATUS=${CASE_STATUS} -DEXPECT_KIND=${CASE_KIND})
    endif()
    foreach(option CLASS LINE COLUMN SIZE EXIT_VALUE NESTING DEPTH SKIP_LABEL)
      if(DEFINED CASE_${option})
        if(option MATCHES "^(CLASS|LINE|COLUMN)$")
          list(APPEND case_args "-DEXPECT_${option}=${CASE_${option}}")
        else()
          list(APPEND case_args "-D${option}=${CASE_${option}}")
        endif()
      endif()
    endforeach()
    if(DEFINED CASE_MESSAGE)
      list(APPEND case_args "-DEXPECT_MESSAGE=${CASE_MESSAGE}")
    endif()
    if(DEFINED CASE_FORBID)
      list(APPEND case_args "-DFORBID_MESSAGE=${CASE_FORBID}")
    endif()
    add_test(NAME ${name}
        COMMAND ${CMAKE_COMMAND} ${case_args} -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli/run_case.cmake)
    set_tests_properties(${name} PROPERTIES TIMEOUT 120)
    if(CASE_SKIP_LABEL)
      set_tests_properties(${name} PROPERTIES SKIP_REGULAR_EXPRESSION "${CASE_SKIP_LABEL} SKIPPED: .+")
    endif()
  endfunction()

  # Positional form used by the code-review round 1 regressions below.
  #   philios_cli_case(<name> <fixture> <status> empty)
  #   philios_cli_case(<name> <fixture> <status> located <class> <line> <column>)
  function(philios_cli_case name fixture status kind)
    if(kind STREQUAL "located")
      philios_cli_test(${name} FIXTURE ${fixture} STATUS ${status} KIND located
          CLASS ${ARGV4} LINE ${ARGV5} COLUMN ${ARGV6})
    else()
      philios_cli_test(${name} FIXTURE ${fixture} STATUS ${status} KIND ${kind})
    endif()
  endfunction()

  # Code-review round 1 regressions. T-LEX-6: `;` is a continuation marker only after code on the
  # same physical line (§3.3); a line of only `;` or a mid-line `;` is a lexical error at the `;`.
  philios_cli_case(t_lex_6_semicolon_only_body_line
      lex6_semicolon_only_body_line.phil 65 located lexical 2 5)
  philios_cli_case(t_lex_6_semicolon_only_module_line
      lex6_semicolon_only_module_line.phil 65 located lexical 1 1)
  philios_cli_case(t_lex_6_semicolon_only_after_comment
      lex6_semicolon_only_after_comment.phil 65 located lexical 2 13)
  philios_cli_case(t_lex_6_semicolon_only_continuation_line
      lex6_semicolon_only_continuation_line.phil 65 located lexical 5 1)
  philios_cli_case(t_lex_6_semicolon_mid_line
      lex6_semicolon_mid_line.phil 65 located lexical 2 9)
  philios_cli_case(t_lex_6_continuation_accepted
      lex6_continuation_accepted.phil 2 empty)
  # T-LEX-7 / T-LEX-3: indentation errors are located at the start of the line, column 1 (§4).
  philios_cli_case(t_lex_7_indent_2_spaces lex7_indent_2_spaces.phil 65 located lexical 2 1)
  philios_cli_case(t_lex_7_indent_3_spaces lex7_indent_3_spaces.phil 65 located lexical 2 1)
  philios_cli_case(t_lex_7_indent_6_spaces lex7_indent_6_spaces.phil 65 located lexical 2 1)
  philios_cli_case(t_lex_7_indent_8_spaces lex7_indent_8_spaces.phil 65 located lexical 2 1)
  philios_cli_case(t_lex_7_inconsistent_dedent
      lex7_inconsistent_dedent.phil 65 located lexical 3 1)
  philios_cli_case(t_lex_3_tab_in_indentation
      lex3_tab_in_indentation.phil 65 located lexical 2 1)
  philios_cli_case(t_lex_3_tab_after_spaces_in_indentation
      lex3_tab_after_spaces_in_indentation.phil 65 located lexical 2 1)
  # T-RUN-5 and the §4 return-type row: a return-type mismatch is reported at the executed
  # `return` statement, not at the function declaration.
  philios_cli_case(t_run_5_main_returns_string run5_main_returns_string.phil 70 located runtime 2 5)
  philios_cli_case(t_run_5_main_returns_null run5_main_returns_null.phil 70 located runtime 2 5)
  philios_cli_case(t_run_5_main_returns_function
      run5_main_returns_function.phil 70 located runtime 4 5)
  philios_cli_case(t_run_5_main_returns_continued
      run5_main_returns_continued.phil 70 located runtime 2 5)
  philios_cli_case(t_run_5_helper_return_mismatch
      run5_helper_return_mismatch.phil 70 located runtime 5 13)

  # The remaining §9 CLI golden acceptance cases (T-CLI-1..7, T-LEX, T-PARSE, T-DECL, T-RUN,
  # T-FN, T-NULL, T-SEC).
  include(${CMAKE_CURRENT_SOURCE_DIR}/tests/cli/cli_cases.cmake)
endif()
