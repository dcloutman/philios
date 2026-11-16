# Shared helpers for the T-BUILD probes (plan 0001 §7, §9). Included by the probe scripts, which
# run with `cmake -P` and receive PHILIOS_SOURCE_DIR and PHILIOS_BINARY_DIR.

foreach(required PHILIOS_SOURCE_DIR PHILIOS_BINARY_DIR)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "${required} is required")
  endif()
endforeach()

# Fails the probe with a message prefixed by the probe name.
macro(probe_fail)
  message(FATAL_ERROR "${PROBE_NAME}: " ${ARGN})
endmacro()

# Loads FACT_<target>_<property> variables written by CMakeLists.txt at generate time.
macro(probe_load_facts)
  set(facts_file "${PHILIOS_BINARY_DIR}/philios_build_facts.cmake")
  if(NOT EXISTS "${facts_file}")
    probe_fail("missing ${facts_file}; configure the build first")
  endif()
  include("${facts_file}")
endmacro()

# Sets out_var to the sorted absolute paths of the project .cpp files under <relative_dir>.
function(probe_project_sources relative_dir out_var)
  file(GLOB_RECURSE files "${PHILIOS_SOURCE_DIR}/${relative_dir}/*.cpp")
  list(SORT files)
  set(${out_var} "${files}" PARENT_SCOPE)
endfunction()

# Sets out_var to the sorted absolute form of a target's SOURCES list.
function(probe_absolute_sources sources out_var)
  set(result "")
  foreach(source IN LISTS sources)
    cmake_path(ABSOLUTE_PATH source BASE_DIRECTORY "${PHILIOS_SOURCE_DIR}" NORMALIZE
        OUTPUT_VARIABLE absolute)
    list(APPEND result "${absolute}")
  endforeach()
  list(SORT result)
  set(${out_var} "${result}" PARENT_SCOPE)
endfunction()

# Reads compile_commands.json. Sets <prefix>_FILES to the list of files and, for each file,
# <prefix>_COMMAND_<index> to its compile command (same index as in <prefix>_FILES).
function(probe_read_compile_database prefix)
  set(database "${PHILIOS_BINARY_DIR}/compile_commands.json")
  if(NOT EXISTS "${database}")
    probe_fail("missing ${database}; CMAKE_EXPORT_COMPILE_COMMANDS must be ON")
  endif()
  file(READ "${database}" json)
  string(JSON count LENGTH "${json}")
  set(files "")
  if(count GREATER 0)
    math(EXPR last "${count} - 1")
    foreach(index RANGE ${last})
      string(JSON file GET "${json}" ${index} file)
      string(JSON command ERROR_VARIABLE no_command GET "${json}" ${index} command)
      if(no_command)
        string(JSON arguments GET "${json}" ${index} arguments)
        set(command "${arguments}")
      endif()
      cmake_path(NORMAL_PATH file)
      list(APPEND files "${file}")
      list(LENGTH files position)
      math(EXPR position "${position} - 1")
      set(${prefix}_COMMAND_${position} " ${command} " PARENT_SCOPE)
    endforeach()
  endif()
  set(${prefix}_FILES "${files}" PARENT_SCOPE)
endfunction()
