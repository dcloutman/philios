# T-BUILD-3 and T-BUILD-4 (plan 0001 §7, §7.1, §9, D44), checked from compile_commands.json and
# the generated build.
#   CHECK=std        T-BUILD-3: every project file compiles with -std=c++20 (not gnu++20).
#   CHECK=separation T-BUILD-4: philios links neither Catch2 nor any file under tests/, and neither
#                    philios nor philios_core has an include directory under tests/. Also checks
#                    the §7 layout: philios_core is a static library built from src/ without
#                    src/main.cpp, and philios is built from src/main.cpp only.
# Inputs: PHILIOS_SOURCE_DIR, PHILIOS_BINARY_DIR, PHILIOS_GENERATOR, CHECK.
cmake_minimum_required(VERSION 3.28)
if(CHECK STREQUAL "std")
  set(PROBE_NAME "T-BUILD-3")
elseif(CHECK STREQUAL "separation")
  set(PROBE_NAME "T-BUILD-4")
else()
  message(FATAL_ERROR "CHECK must be 'std' or 'separation', got '${CHECK}'")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/probe_common.cmake")

probe_project_sources(src src_sources)
probe_project_sources(tests/unit unit_sources)
set(project_sources ${src_sources} ${unit_sources})
probe_read_compile_database(db)

# Every project source has exactly one compile-database entry.
foreach(source IN LISTS project_sources)
  set(matches 0)
  foreach(file IN LISTS db_FILES)
    if(file STREQUAL source)
      math(EXPR matches "${matches} + 1")
    endif()
  endforeach()
  if(NOT matches EQUAL 1)
    probe_fail("${source} has ${matches} compile_commands.json entries, expected 1")
  endif()
endforeach()

set(tests_dir "${PHILIOS_SOURCE_DIR}/tests")

if(CHECK STREQUAL "std")
  set(checked 0)
  set(index 0)
  foreach(file IN LISTS db_FILES)
    set(command "${db_COMMAND_${index}}")
    math(EXPR index "${index} + 1")
    if(NOT file IN_LIST project_sources)
      continue()
    endif()
    # Exactly one language-standard flag, and it is -std=c++20 (so no gnu++20 or later override).
    string(REGEX MATCHALL "[ \"]-std=[^ \"]+" std_flags "${command}")
    list(TRANSFORM std_flags STRIP)
    list(TRANSFORM std_flags REPLACE "^\"" "")
    if(NOT std_flags STREQUAL "-std=c++20")
      probe_fail("${file} has standard flags '${std_flags}', expected only -std=c++20:\n${command}")
    endif()
    math(EXPR checked "${checked} + 1")
  endforeach()
  message(STATUS "T-BUILD-3: all ${checked} project files compile with -std=c++20")
  return()
endif()

# CHECK=separation.
probe_load_facts()

if(NOT FACT_philios_core_TYPE STREQUAL "STATIC_LIBRARY")
  probe_fail("philios_core is '${FACT_philios_core_TYPE}', expected STATIC_LIBRARY (§7)")
endif()
if(NOT FACT_philios_TYPE STREQUAL "EXECUTABLE")
  probe_fail("philios is '${FACT_philios_TYPE}', expected EXECUTABLE (§7)")
endif()
probe_absolute_sources("${FACT_philios_SOURCES}" philios_sources)
if(NOT philios_sources STREQUAL "${PHILIOS_SOURCE_DIR}/src/main.cpp")
  probe_fail("philios sources must be src/main.cpp only, got: ${philios_sources}")
endif()
probe_absolute_sources("${FACT_philios_core_SOURCES}" core_sources)
set(expected_core "${src_sources}")
list(REMOVE_ITEM expected_core "${PHILIOS_SOURCE_DIR}/src/main.cpp")
if(NOT core_sources STREQUAL expected_core)
  probe_fail("philios_core sources must be every src/ .cpp except src/main.cpp.\n"
             "Target: ${core_sources}\nExpected: ${expected_core}")
endif()

# Link dependencies: philios links philios_core only; philios_core links nothing.
if(NOT FACT_philios_LINK_LIBRARIES STREQUAL "philios_core")
  probe_fail("philios links '${FACT_philios_LINK_LIBRARIES}', expected 'philios_core' only")
endif()
foreach(property IN ITEMS LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
  if(NOT FACT_philios_core_${property} STREQUAL "")
    probe_fail("philios_core ${property} is '${FACT_philios_core_${property}}', expected empty")
  endif()
endforeach()

# Include directories: no tests/ path on philios or philios_core.
foreach(target IN ITEMS philios_core philios)
  foreach(directory IN LISTS FACT_${target}_INCLUDE_DIRECTORIES)
    cmake_path(IS_PREFIX tests_dir "${directory}" NORMALIZE under_tests)
    string(TOLOWER "${directory}" lower)
    if(under_tests OR lower MATCHES "catch2")
      probe_fail("${target} has include directory ${directory}")
    endif()
  endforeach()
endforeach()

# Effective compile commands of every src/ file (philios_core and philios).
set(index 0)
set(checked 0)
foreach(file IN LISTS db_FILES)
  set(command "${db_COMMAND_${index}}")
  math(EXPR index "${index} + 1")
  if(NOT file IN_LIST src_sources)
    continue()
  endif()
  string(FIND "${command}" "${tests_dir}/" tests_at)
  string(TOLOWER "${command}" lower)
  if(tests_at GREATER_EQUAL 0 OR lower MATCHES "catch2")
    probe_fail("compile command for ${file} references tests/ or Catch2:\n${command}")
  endif()
  math(EXPR checked "${checked} + 1")
endforeach()

# Generated link command of philios.
if(NOT PHILIOS_GENERATOR STREQUAL "Unix Makefiles")
  probe_fail("generated-build inspection supports the 'Unix Makefiles' generator only "
             "(plan §7 target environment); this build uses '${PHILIOS_GENERATOR}'")
endif()
set(link_file "${PHILIOS_BINARY_DIR}/CMakeFiles/philios.dir/link.txt")
if(NOT EXISTS "${link_file}")
  probe_fail("missing generated link command ${link_file}")
endif()
file(READ "${link_file}" link_command)
string(TOLOWER "${link_command}" lower)
string(FIND "${link_command}" "/tests/" link_tests_at)
if(lower MATCHES "catch2" OR link_tests_at GREATER_EQUAL 0 OR lower MATCHES "philios_tests")
  probe_fail("philios link command references Catch2 or tests/:\n${link_command}")
endif()
string(REGEX MATCHALL "[^ \t\r\n]+\\.(o|a|so)( |$)" link_inputs "${link_command}")
foreach(input IN LISTS link_inputs)
  string(STRIP "${input}" input)
  if(NOT input MATCHES "(^|/)CMakeFiles/philios\\.dir/src/main\\.cpp\\.o$"
     AND NOT input MATCHES "(^|/)libphilios_core\\.a$")
    probe_fail("unexpected philios link input '${input}':\n${link_command}")
  endif()
endforeach()
message(STATUS "T-BUILD-4: philios_core is static; philios links only main.cpp.o and "
               "libphilios_core.a; ${checked} src/ compile commands and both targets' include "
               "directories have no tests/ or Catch2 path")
