# T-BUILD-5 (plan 0001 §7, §9, D36): the test-only cert-err58-cpp suppression in
# tests/unit/.clang-tidy must not reach src/.
#   (a) the configuration clang-tidy resolves for src/main.cpp enables cert-err58-cpp;
#   (b) the configuration for a file under tests/unit/ disables cert-err58-cpp and nothing else;
#   (c) clang-tidy with the src/ configuration fails on tests/build/dynamic_static_init.cpp with a
#       cert-err58-cpp diagnostic.
# Inputs: PHILIOS_SOURCE_DIR, PHILIOS_BINARY_DIR (holds compile_commands.json), CLANG_TIDY_EXE.
cmake_minimum_required(VERSION 3.28)

foreach(required PHILIOS_SOURCE_DIR PHILIOS_BINARY_DIR CLANG_TIDY_EXE)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "T-BUILD-5: ${required} is required")
  endif()
endforeach()

set(src_file "${PHILIOS_SOURCE_DIR}/src/main.cpp")
set(fixture "${PHILIOS_SOURCE_DIR}/tests/build/dynamic_static_init.cpp")
file(GLOB_RECURSE unit_files "${PHILIOS_SOURCE_DIR}/tests/unit/*.cpp")
list(SORT unit_files)
if(NOT unit_files)
  message(FATAL_ERROR "T-BUILD-5: no file found under tests/unit/")
endif()
list(GET unit_files 0 unit_file)
foreach(path IN ITEMS "${src_file}" "${fixture}" "${unit_file}")
  if(NOT EXISTS "${path}")
    message(FATAL_ERROR "T-BUILD-5: missing file ${path}")
  endif()
endforeach()

# Runs clang-tidy and returns its stdout; any nonzero status fails the probe.
function(run_tidy out_var)
  execute_process(
      COMMAND "${CLANG_TIDY_EXE}" -p "${PHILIOS_BINARY_DIR}" ${ARGN}
      WORKING_DIRECTORY "${PHILIOS_SOURCE_DIR}"
      INPUT_FILE /dev/null
      OUTPUT_VARIABLE out
      ERROR_VARIABLE err
      RESULT_VARIABLE status
      TIMEOUT 120)
  if(NOT status EQUAL 0)
    message(FATAL_ERROR "T-BUILD-5: clang-tidy ${ARGN} failed (${status}):\n${out}\n${err}")
  endif()
  set(${out_var} "${out}" PARENT_SCOPE)
endfunction()

# Returns the enabled check names (one list entry per check).
function(enabled_checks file out_var)
  run_tidy(listing --list-checks "${file}")
  string(REGEX MATCHALL "\n[ \t]+[a-z0-9.-]+" lines "${listing}")
  set(checks "")
  foreach(line IN LISTS lines)
    string(STRIP "${line}" check)
    list(APPEND checks "${check}")
  endforeach()
  list(SORT checks)
  set(${out_var} "${checks}" PARENT_SCOPE)
endfunction()

# Returns the Checks value from --dump-config.
function(checks_value dump out_var)
  if(NOT dump MATCHES "\nChecks:[ \t]*'([^']*)'")
    message(FATAL_ERROR "T-BUILD-5: no Checks entry in dumped configuration:\n${dump}")
  endif()
  set(${out_var} "${CMAKE_MATCH_1}" PARENT_SCOPE)
endfunction()

# (a) src/ configuration enables cert-err58-cpp.
run_tidy(src_dump --dump-config "${src_file}")
checks_value("${src_dump}" src_checks)
enabled_checks("${src_file}" src_enabled)
if(NOT "cert-err58-cpp" IN_LIST src_enabled)
  message(FATAL_ERROR "T-BUILD-5 (a): cert-err58-cpp is not enabled for src/main.cpp "
                      "(Checks: '${src_checks}')")
endif()
if(src_checks MATCHES "-cert-err58-cpp")
  message(FATAL_ERROR "T-BUILD-5 (a): src/ configuration removes cert-err58-cpp: '${src_checks}'")
endif()
message(STATUS "T-BUILD-5 (a): src/main.cpp Checks '${src_checks}' enables cert-err58-cpp")

# (b) tests/unit/ configuration disables cert-err58-cpp and nothing else.
run_tidy(unit_dump --dump-config "${unit_file}")
checks_value("${unit_dump}" unit_checks)
if(NOT unit_checks STREQUAL "${src_checks},-cert-err58-cpp")
  message(FATAL_ERROR "T-BUILD-5 (b): ${unit_file} Checks '${unit_checks}' is not the src/ "
                      "value '${src_checks}' plus ',-cert-err58-cpp'")
endif()
string(REGEX REPLACE "\nChecks:[^\n]*" "" src_rest "${src_dump}")
string(REGEX REPLACE "\nChecks:[^\n]*" "" unit_rest "${unit_dump}")
if(NOT src_rest STREQUAL unit_rest)
  message(FATAL_ERROR "T-BUILD-5 (b): tests/unit/ configuration differs from src/ in more than "
                      "Checks.\nsrc/:\n${src_dump}\ntests/unit/:\n${unit_dump}")
endif()
enabled_checks("${unit_file}" unit_enabled)
set(expected_unit "${src_enabled}")
list(REMOVE_ITEM expected_unit "cert-err58-cpp")
if(NOT unit_enabled STREQUAL expected_unit)
  message(FATAL_ERROR "T-BUILD-5 (b): enabled checks for ${unit_file} are not exactly the src/ "
                      "checks minus cert-err58-cpp")
endif()
list(LENGTH src_enabled src_count)
list(LENGTH unit_enabled unit_count)
message(STATUS "T-BUILD-5 (b): tests/unit/ enables ${unit_count} of the ${src_count} src/ checks; "
               "only cert-err58-cpp is disabled")

# (c) The src/ configuration fails on the fixture with cert-err58-cpp.
set(config_file "${PHILIOS_BINARY_DIR}/t_build_5_src_clang_tidy.yaml")
file(WRITE "${config_file}" "${src_dump}")
execute_process(
    COMMAND "${CLANG_TIDY_EXE}" "--config-file=${config_file}" "${fixture}" -- -std=c++20
    WORKING_DIRECTORY "${PHILIOS_SOURCE_DIR}"
    INPUT_FILE /dev/null
    OUTPUT_VARIABLE fixture_out
    ERROR_VARIABLE fixture_err
    RESULT_VARIABLE fixture_status
    TIMEOUT 120)
if(fixture_status EQUAL 0)
  message(FATAL_ERROR "T-BUILD-5 (c): clang-tidy accepted the fixture:\n${fixture_out}\n${fixture_err}")
endif()
if(NOT fixture_out MATCHES "dynamic_static_init\\.cpp:[0-9]+:[0-9]+: error: [^\n]*\\[cert-err58-cpp")
  message(FATAL_ERROR "T-BUILD-5 (c): no cert-err58-cpp error for the fixture "
                      "(status ${fixture_status}):\n${fixture_out}\n${fixture_err}")
endif()
message(STATUS "T-BUILD-5 (c): src/ configuration rejects the fixture with cert-err58-cpp")
