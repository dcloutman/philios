cmake_minimum_required(VERSION 3.28)

if(NOT DEFINED PHILIOS_SOURCE_DIR)
  message(FATAL_ERROR "PHILIOS_SOURCE_DIR is required")
endif()

set(kIncludePattern "^[ \t]*#[ \t]*include[ \t]*[<\"][ \t]*([^>\"]+)[ \t]*[>\"]")

function(extract_header include_line out_var)
  if("${include_line}" MATCHES "${kIncludePattern}")
    set(header_raw "${CMAKE_MATCH_1}")
    string(REGEX REPLACE "[ \t]" "" header "${header_raw}")
    set(${out_var} "${header}" PARENT_SCOPE)
  else()
    set(${out_var} "" PARENT_SCOPE)
  endif()
endfunction()

set(self_check_spellings
    "#include <fcntl.h>"
    "#  include < sys/fcntl.h >"
    "# include \"unistd.h\""
    "# include < sys/unistd.h >"
    "# include <sys/stat.h>"
    "# include <sys/syscall.h>")

foreach(spelling IN LISTS self_check_spellings)
  extract_header("${spelling}" header)
  if(header STREQUAL "")
    message(FATAL_ERROR "include-matching pattern did not match spelling: ${spelling}")
  endif()
endforeach()

set(posix_headers
    "fcntl.h"
    "sys/fcntl.h"
    "unistd.h"
    "sys/unistd.h"
    "sys/stat.h")

set(syscall_header "sys/syscall.h")
set(allowed_source_file "${PHILIOS_SOURCE_DIR}/src/cli/source_file.cpp")

file(GLOB_RECURSE source_files
    "${PHILIOS_SOURCE_DIR}/src/*.cpp"
    "${PHILIOS_SOURCE_DIR}/src/*.hpp")

foreach(source_file IN LISTS source_files)
  file(STRINGS "${source_file}" lines)
  foreach(line IN LISTS lines)
    extract_header("${line}" header)
    if(header STREQUAL "")
      continue()
    endif()

    if(header STREQUAL "${syscall_header}")
      message(FATAL_ERROR "${source_file} includes forbidden header <${syscall_header}>")
    endif()

    list(FIND posix_headers "${header}" posix_index)
    if(posix_index GREATER -1 AND NOT source_file STREQUAL allowed_source_file)
      message(FATAL_ERROR
          "${source_file} includes POSIX loading header <${header}> outside src/cli/source_file.cpp")
    endif()
  endforeach()
endforeach()
