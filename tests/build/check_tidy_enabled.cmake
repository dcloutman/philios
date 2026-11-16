# T-BUILD-1 (plan 0001 §7, §9): with PHILIOS_CLANG_TIDY=ON, CXX_CLANG_TIDY is set on all three
# project targets and the generated build runs clang-tidy on every project source, so every `src/`
# file compiled into philios_core and every test file is analyzed during the normal build.
# `.clang-tidy` sets WarningsAsErrors '*', so any diagnostic fails that build; a successful clean
# build followed by this probe is the T-BUILD-1 acceptance check.
# Inputs: PHILIOS_SOURCE_DIR, PHILIOS_BINARY_DIR, PHILIOS_GENERATOR, CLANG_TIDY_EXE.
cmake_minimum_required(VERSION 3.28)
set(PROBE_NAME "T-BUILD-1")
include("${CMAKE_CURRENT_LIST_DIR}/probe_common.cmake")

if(NOT DEFINED CLANG_TIDY_EXE OR NOT EXISTS "${CLANG_TIDY_EXE}")
  probe_fail("CLANG_TIDY_EXE '${CLANG_TIDY_EXE}' does not exist")
endif()
probe_load_facts()

# WarningsAsErrors '*' in the root configuration makes every diagnostic fail the build.
file(STRINGS "${PHILIOS_SOURCE_DIR}/.clang-tidy" warnings_as_errors REGEX "^WarningsAsErrors:")
if(NOT warnings_as_errors MATCHES "^WarningsAsErrors:[ \t]*'\\*'[ \t]*$")
  probe_fail(".clang-tidy must set WarningsAsErrors: '*' (found '${warnings_as_errors}')")
endif()

# Every project source belongs to exactly the expected target.
probe_project_sources(src src_sources)
probe_project_sources(tests/unit unit_sources)
set(expected_core "${src_sources}")
list(REMOVE_ITEM expected_core "${PHILIOS_SOURCE_DIR}/src/main.cpp")
set(expected_philios "${PHILIOS_SOURCE_DIR}/src/main.cpp")
foreach(target IN ITEMS philios_core philios philios_tests)
  probe_absolute_sources("${FACT_${target}_SOURCES}" sources_${target})
endforeach()
foreach(pair IN ITEMS "philios_core:expected_core" "philios:expected_philios"
                      "philios_tests:unit_sources")
  string(REPLACE ":" ";" pair "${pair}")
  list(GET pair 0 target)
  list(GET pair 1 expected_var)
  if(NOT sources_${target} STREQUAL ${expected_var})
    probe_fail("${target} sources differ from the expected project files.\n"
               "Target: ${sources_${target}}\nExpected: ${${expected_var}}")
  endif()
endforeach()

if(NOT PHILIOS_GENERATOR STREQUAL "Unix Makefiles")
  probe_fail("generated-build inspection supports the 'Unix Makefiles' generator only "
             "(plan §7 target environment); this build uses '${PHILIOS_GENERATOR}'")
endif()

set(analyzed 0)
foreach(target IN ITEMS philios_core philios philios_tests)
  if(NOT FACT_${target}_CXX_CLANG_TIDY STREQUAL CLANG_TIDY_EXE)
    probe_fail("CXX_CLANG_TIDY on ${target} is '${FACT_${target}_CXX_CLANG_TIDY}', "
               "expected '${CLANG_TIDY_EXE}'")
  endif()
  set(build_make "${PHILIOS_BINARY_DIR}/CMakeFiles/${target}.dir/build.make")
  if(NOT EXISTS "${build_make}")
    probe_fail("missing generated build file ${build_make}")
  endif()
  file(STRINGS "${build_make}" compile_lines REGEX " -c ")
  foreach(source IN LISTS sources_${target})
    set(found FALSE)
    foreach(line IN LISTS compile_lines)
      string(FIND "${line}" "--source=${source} " source_at)
      if(source_at GREATER_EQUAL 0)
        string(FIND "${line}" "__run_co_compile --tidy=\"${CLANG_TIDY_EXE};" tidy_at)
        if(tidy_at GREATER_EQUAL 0)
          set(found TRUE)
          break()
        endif()
      endif()
    endforeach()
    if(NOT found)
      probe_fail("the generated compile rule for ${source} (${target}) does not run clang-tidy")
    endif()
    math(EXPR analyzed "${analyzed} + 1")
  endforeach()
endforeach()

list(LENGTH src_sources src_count)
list(LENGTH unit_sources unit_count)
math(EXPR expected_count "${src_count} + ${unit_count}")
if(NOT analyzed EQUAL expected_count)
  probe_fail("analyzed ${analyzed} sources, expected ${expected_count}")
endif()
message(STATUS "T-BUILD-1: clang-tidy runs on all ${analyzed} project sources "
               "(philios_core, philios, philios_tests) with WarningsAsErrors '*'")
