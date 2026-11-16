# T-BUILD-2 (plan 0001 §7, §9): configuring with PHILIOS_CLANG_TIDY=ON when clang-tidy is not
# installed fails with a message naming the missing tool and how to disable the option.
# The probe configures this project into a scratch directory with every program search rerooted
# into an empty directory (CMAKE_FIND_ROOT_PATH with CMAKE_FIND_ROOT_PATH_MODE_PROGRAM ONLY), so
# find_program cannot find clang-tidy. The compiler and make program are passed by full path.
# Inputs: PHILIOS_SOURCE_DIR, PHILIOS_BINARY_DIR, PHILIOS_GENERATOR, PHILIOS_MAKE_PROGRAM,
#         PHILIOS_CXX_COMPILER.
cmake_minimum_required(VERSION 3.28)
set(PROBE_NAME "T-BUILD-2")
include("${CMAKE_CURRENT_LIST_DIR}/probe_common.cmake")

foreach(required PHILIOS_GENERATOR PHILIOS_CXX_COMPILER)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    probe_fail("${required} is required")
  endif()
endforeach()

set(scratch "${PHILIOS_BINARY_DIR}/t_build_2_missing_clang_tidy")
file(REMOVE_RECURSE "${scratch}")
file(MAKE_DIRECTORY "${scratch}/empty_root")

set(configure_args
    -S "${PHILIOS_SOURCE_DIR}" -B "${scratch}/build" -G "${PHILIOS_GENERATOR}"
    "-DCMAKE_CXX_COMPILER=${PHILIOS_CXX_COMPILER}"
    -DPHILIOS_CLANG_TIDY=ON
    "-DCMAKE_FIND_ROOT_PATH=${scratch}/empty_root"
    -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=ONLY)
if(DEFINED PHILIOS_MAKE_PROGRAM AND NOT PHILIOS_MAKE_PROGRAM STREQUAL "")
  list(APPEND configure_args "-DCMAKE_MAKE_PROGRAM=${PHILIOS_MAKE_PROGRAM}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" ${configure_args}
    INPUT_FILE /dev/null
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err
    RESULT_VARIABLE status
    TIMEOUT 240)
set(log "${out}\n${err}")
set(cache "")
if(EXISTS "${scratch}/build/CMakeCache.txt")
  file(READ "${scratch}/build/CMakeCache.txt" cache LIMIT 1048576)
endif()
file(REMOVE_RECURSE "${scratch}")

if(status EQUAL 0)
  probe_fail("configuration succeeded without clang-tidy:\n${log}")
endif()
if(NOT cache MATCHES "\nCLANG_TIDY_EXE:FILEPATH=CLANG_TIDY_EXE-NOTFOUND\n")
  probe_fail("the scratch configuration did not reach a not-found clang-tidy search:\n${log}")
endif()
# CMake wraps long messages, so compare with whitespace collapsed.
string(REGEX REPLACE "[ \t\r\n]+" " " flat "${log}")
foreach(expected IN ITEMS "PHILIOS_CLANG_TIDY is ON but clang-tidy was not found"
                          "-DPHILIOS_CLANG_TIDY=OFF")
  string(FIND "${flat}" "${expected}" at)
  if(at LESS 0)
    probe_fail("configure output lacks '${expected}':\n${log}")
  endif()
endforeach()
message(STATUS "T-BUILD-2: configuration without clang-tidy failed (status ${status}) "
               "naming clang-tidy and -DPHILIOS_CLANG_TIDY=OFF")
