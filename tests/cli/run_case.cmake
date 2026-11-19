# CLI golden-test driver (plan 0001 §7, §9; plan 0002 §7 task 1). Runs the real `philios` binary
# in a fresh working directory with execute_process(WORKING_DIRECTORY ... TIMEOUT <limit>) and
# checks the exit status, the stderr text (a class/line/column prefix, the usage text, a startup
# prefix, or empty on success), and stdout: by default stdout must be empty; with
# EXPECT_STDOUT_HEX it must equal those exact bytes. Large and structural inputs are generated
# here, at test time, into the build directory; they are never committed. The run-time limit
# (TIMEOUT) is 10 seconds for every MODE except `long_line`, which gets 20 seconds (plan 0002
# architecture decisions §6 item 8: its 2 MiB input takes 8 to 9.3 seconds under ASan).
#
# Invoked as `cmake -D<var>=<value> ... -P run_case.cmake`. Variables:
#   PHILIOS_BIN      Absolute path of the philios executable (required).
#   WORK_DIR         Directory the case runs in; deleted and recreated first (required).
#   MODE             How main.phil is prepared (required):
#                      file            copy FIXTURE to main.phil
#                      none            no main.phil
#                      directory       main.phil is a directory
#                      fifo            main.phil is a FIFO (a hang fails through the timeout)
#                      symlink         main.phil is a symlink to a copy of FIXTURE
#                      dangling        main.phil is a symlink to a missing file
#                      unreadable      copy FIXTURE, then chmod 000 (skipped when run as root)
#                      json            copy FIXTURE and add a philios.json containing `{`
#                      sized           a program returning EXIT_VALUE, padded with comment lines
#                                      to exactly SIZE bytes
#                      nesting         `main` returns nested calls reaching parser nesting NESTING
#                      deep_open       100,000 nested `f(` with no closing parentheses
#                      deep_calls      100,000 nested well-formed calls
#                      long_line       a single-line call statement; the file is SIZE bytes
#                      long_identifier a call of one identifier; the file is SIZE bytes
#                      call_chain      main -> f2 -> ... -> fDEPTH (call depth DEPTH)
#                      eval_depth      the T-SEC-4 worst case (calls nested to the parser limit)
#                      pseudo_nesting  `main`'s body is `echo(` followed by NESTING - 2 nested
#                                      `typeof(`, the string "x", and the closing parentheses, so
#                                      the innermost argument list is at nesting level NESTING
#                                      (plan 0002 T2-TYPEOF-6)
#                      broken_pipe_program
#                                      the plan 0002 T2-ECHO-6 program: `spam(s)` echoes its
#                                      argument and recurses; `main` calls it with a string of
#                                      65,536 `x` characters
#                      copy_folder     copy every regular file directly inside FIXTURE_DIR (for
#                                      example main.phil and philios.json) into WORK_DIR under the
#                                      same names; subdirectories are not copied (plan 0002 task 6,
#                                      T2-DOC-1; architecture decisions §6 item 7)
#   FIXTURE          Source fixture for the file-based modes.
#   FIXTURE_DIR      Absolute path of the source directory for MODE copy_folder. It is only read,
#                    never modified, and must be an existing directory. The driver fails before
#                    deleting anything if FIXTURE_DIR is missing, is not a directory, or is the same
#                    as, inside, or contains WORK_DIR. The check compares canonical paths
#                    (file(REAL_PATH)), so a symlink in either path, including in an existing
#                    parent of a WORK_DIR that does not exist yet, cannot hide an overlap. A WORK_DIR
#                    whose missing part contains `.`, `..`, or a dangling symlink is rejected.
#   ARGS             Command-line arguments (a CMake list); default none.
#   EXPECT_STATUS    Required exit status, unless EXPECT_NO_CRASH is set.
#   EXPECT_KIND      What stderr must hold:
#                      empty     nothing (a successful run)
#                      located   exactly one line starting `main.phil:LINE:COLUMN: CLASS error: `
#                      startup   exactly one line starting `philios: startup error: main.phil: `
#                      usage     exactly the two-line usage error
#   EXPECT_CLASS, EXPECT_LINE, EXPECT_COLUMN   For EXPECT_KIND=located. An empty EXPECT_COLUMN
#                    checks the class and line only (used only where the plan leaves the column open).
#   EXPECT_MESSAGE   Optional text the diagnostic must contain.
#   FORBID_MESSAGE   Optional text the diagnostic must not contain.
#   EXPECT_NO_CRASH  If true, only require a normal interpreter status (0, 65, 66, or 70), no
#                    sanitizer report, and empty stdout (T-PARSE-5).
#   EXPECT_STDOUT_HEX
#                    The exact expected stdout bytes as lowercase hexadecimal (plan 0002 §8: "stdout
#                    `text`"). stdout is captured to a file with OUTPUT_FILE, read back with
#                    file(READ ... HEX), and compared exactly. When unset, stdout must be empty.
#                    Valid only with STDOUT_MODE capture.
#   STDOUT_MODE      Where the binary's stdout goes (plan 0002 §7 task 1); default `capture`:
#                      capture      stdout is captured and checked (EXPECT_STDOUT_HEX, or empty)
#                      dev_full     stdout is /dev/full; when /dev/full does not exist the driver
#                                   writes `<SKIP_LABEL> SKIPPED: /dev/full not available` and the
#                                   case is reported as skipped through SKIP_REGULAR_EXPRESSION
#                      closed       the binary runs with descriptor 1 closed, through
#                                   `sh -c 'exec "$0" >&-' <binary>`
#                      broken_pipe  the binary's stdout is a pipe to `${CMAKE_COMMAND} -E true`,
#                                   built with two COMMANDs in one execute_process; the binary's
#                                   own status is taken from RESULTS_VARIABLE
#                    In dev_full, closed, and broken_pipe, stdout is not checked.
#   SKIP_LABEL       Test label used in the skip line `<SKIP_LABEL> SKIPPED: <reason>`.

cmake_minimum_required(VERSION 3.28)

foreach(required PHILIOS_BIN WORK_DIR MODE)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "run_case.cmake: ${required} is required")
  endif()
endforeach()

if(NOT DEFINED STDOUT_MODE OR STDOUT_MODE STREQUAL "")
  set(STDOUT_MODE capture)
endif()
if(NOT STDOUT_MODE MATCHES "^(capture|dev_full|closed|broken_pipe)$")
  message(FATAL_ERROR "run_case.cmake: unknown STDOUT_MODE '${STDOUT_MODE}'")
endif()
if(DEFINED EXPECT_STDOUT_HEX AND NOT STDOUT_MODE STREQUAL "capture")
  message(FATAL_ERROR "run_case.cmake: EXPECT_STDOUT_HEX needs STDOUT_MODE capture")
endif()

if(STDOUT_MODE STREQUAL "dev_full" AND NOT EXISTS "/dev/full")
  message("${SKIP_LABEL} SKIPPED: /dev/full not available")
  return()
endif()

# MODE copy_folder only reads FIXTURE_DIR. Check it before WORK_DIR is deleted, so that a wrong
# path can never remove or write into the source directory.
if(MODE STREQUAL "copy_folder")
  if(NOT DEFINED FIXTURE_DIR OR NOT IS_DIRECTORY "${FIXTURE_DIR}")
    message(FATAL_ERROR
            "run_case.cmake: MODE copy_folder needs an existing directory FIXTURE_DIR (got '${FIXTURE_DIR}')")
  endif()
  file(REAL_PATH "${FIXTURE_DIR}" source_dir_real)
  # Compare canonical paths. WORK_DIR may not exist yet, so resolve its deepest existing ancestor
  # with file(REAL_PATH) (this follows a symlink anywhere in the existing part) and append the
  # missing components unchanged. A missing component that is `.`, `..`, or a dangling symlink
  # could still lead elsewhere when the directory is created, so it is rejected.
  cmake_path(ABSOLUTE_PATH WORK_DIR OUTPUT_VARIABLE work_dir_existing)
  set(work_dir_missing "")
  while(NOT EXISTS "${work_dir_existing}")
    if(IS_SYMLINK "${work_dir_existing}")
      message(FATAL_ERROR
              "run_case.cmake: WORK_DIR '${WORK_DIR}' must not go through a dangling symlink ('${work_dir_existing}')")
    endif()
    cmake_path(GET work_dir_existing FILENAME work_dir_name)
    if(work_dir_name STREQUAL "." OR work_dir_name STREQUAL "..")
      message(FATAL_ERROR
              "run_case.cmake: the missing part of WORK_DIR '${WORK_DIR}' must not contain '.' or '..'")
    endif()
    if(NOT work_dir_name STREQUAL "")
      set(work_dir_missing "${work_dir_name}/${work_dir_missing}")
    endif()
    cmake_path(GET work_dir_existing PARENT_PATH work_dir_existing)
  endwhile()
  file(REAL_PATH "${work_dir_existing}" work_dir_real)
  if(NOT work_dir_missing STREQUAL "")
    cmake_path(APPEND work_dir_real "${work_dir_missing}")
  endif()
  cmake_path(IS_PREFIX source_dir_real "${work_dir_real}" NORMALIZE work_in_source)
  cmake_path(IS_PREFIX work_dir_real "${source_dir_real}" NORMALIZE source_in_work)
  if(work_in_source OR source_in_work)
    message(FATAL_ERROR
            "run_case.cmake: WORK_DIR '${WORK_DIR}' must not be, contain, or lie inside FIXTURE_DIR '${FIXTURE_DIR}'")
  endif()
endif()

set(main_file "${WORK_DIR}/main.phil")
file(REMOVE_RECURSE "${WORK_DIR}")
file(MAKE_DIRECTORY "${WORK_DIR}")

# Copies FIXTURE byte for byte to @p destination.
function(copy_fixture destination)
  if(NOT DEFINED FIXTURE OR NOT EXISTS "${FIXTURE}")
    message(FATAL_ERROR "run_case.cmake: MODE ${MODE} needs an existing FIXTURE (got '${FIXTURE}')")
  endif()
  file(COPY_FILE "${FIXTURE}" "${destination}")
endfunction()

# Fails unless the generated main.phil has exactly SIZE bytes.
function(check_generated_size)
  file(SIZE "${main_file}" written_size)
  if(NOT written_size EQUAL SIZE)
    message(FATAL_ERROR "run_case.cmake: generated ${written_size} bytes, expected ${SIZE}")
  endif()
endfunction()

if(MODE STREQUAL "file")
  copy_fixture("${main_file}")
elseif(MODE STREQUAL "none")
  # Nothing to prepare.
elseif(MODE STREQUAL "directory")
  file(MAKE_DIRECTORY "${main_file}")
elseif(MODE STREQUAL "fifo")
  execute_process(COMMAND mkfifo "${main_file}" RESULT_VARIABLE fifo_result)
  if(NOT fifo_result EQUAL 0)
    message(FATAL_ERROR "run_case.cmake: mkfifo failed: ${fifo_result}")
  endif()
elseif(MODE STREQUAL "symlink")
  copy_fixture("${WORK_DIR}/target.phil")
  file(CREATE_LINK "target.phil" "${main_file}" SYMBOLIC)
elseif(MODE STREQUAL "dangling")
  file(CREATE_LINK "missing.phil" "${main_file}" SYMBOLIC)
elseif(MODE STREQUAL "unreadable")
  execute_process(COMMAND id -u OUTPUT_VARIABLE user_id OUTPUT_STRIP_TRAILING_WHITESPACE
                  RESULT_VARIABLE id_result)
  if(NOT id_result EQUAL 0)
    message(FATAL_ERROR "run_case.cmake: id -u failed: ${id_result}")
  endif()
  if(user_id STREQUAL "0")
    message("${SKIP_LABEL} SKIPPED: running as root, which can read a mode-000 file")
    return()
  endif()
  copy_fixture("${main_file}")
  execute_process(COMMAND chmod 000 "${main_file}" RESULT_VARIABLE chmod_result)
  if(NOT chmod_result EQUAL 0)
    message(FATAL_ERROR "run_case.cmake: chmod 000 failed: ${chmod_result}")
  endif()
elseif(MODE STREQUAL "json")
  copy_fixture("${main_file}")
  file(WRITE "${WORK_DIR}/philios.json" "{")
elseif(MODE STREQUAL "sized")
  # A valid program padded with comment lines to exactly SIZE bytes.
  set(program "function main():\n    return ${EXIT_VALUE}\n")
  string(LENGTH "${program}" program_length)
  math(EXPR remaining "${SIZE} - ${program_length}")
  set(line_body_length 1000)
  # Each full padding line is `//`, the body, and an LF.
  math(EXPR full_line_length "${line_body_length} + 3")
  math(EXPR full_lines "${remaining} / ${full_line_length}")
  math(EXPR last_line_length "${remaining} % ${full_line_length}")
  string(REPEAT "x" ${line_body_length} line_body)
  string(REPEAT "//${line_body}\n" ${full_lines} padding)
  if(last_line_length GREATER_EQUAL 3)
    math(EXPR last_body_length "${last_line_length} - 3")
    string(REPEAT "x" ${last_body_length} last_body)
    string(APPEND padding "//${last_body}\n")
  elseif(last_line_length GREATER 0)
    string(REPEAT "\n" ${last_line_length} last_blank)
    string(APPEND padding "${last_blank}")
  endif()
  file(WRITE "${main_file}" "${program}${padding}")
  check_generated_size()
elseif(MODE STREQUAL "nesting")
  # `main`'s body is nesting level 1 and each argument list adds one level, so
  # `return f(f(...f(7)...))` with N calls reaches level N + 1 (§3.4).
  math(EXPR call_count "${NESTING} - 1")
  string(REPEAT "f(" ${call_count} opens)
  string(REPEAT ")" ${call_count} closes)
  file(WRITE "${main_file}"
       "function f(x: Integer): Integer:\n    return x\nfunction main():\n    return ${opens}7${closes}\n")
elseif(MODE STREQUAL "deep_open")
  string(REPEAT "f(" 100000 opens)
  file(WRITE "${main_file}"
       "function f(x: Integer): Integer:\n    return x\nfunction main():\n    return ${opens}\n")
elseif(MODE STREQUAL "deep_calls")
  string(REPEAT "f(" 100000 opens)
  string(REPEAT ")" 100000 closes)
  file(WRITE "${main_file}"
       "function f(x: Integer): Integer:\n    return x\nfunction main():\n    return ${opens}1${closes}\n")
elseif(MODE STREQUAL "long_line")
  set(head "function f(): null:\n    pass\nfunction main():\n    f(")
  set(tail "1)\n")
  string(LENGTH "${head}${tail}" fixed_length)
  math(EXPR argument_count "(${SIZE} - ${fixed_length}) / 3")
  math(EXPR filler_length "${SIZE} - ${fixed_length} - ${argument_count} * 3")
  string(REPEAT "1, " ${argument_count} arguments)
  string(REPEAT " " ${filler_length} filler)
  file(WRITE "${main_file}" "${head}${arguments}${filler}${tail}")
  check_generated_size()
elseif(MODE STREQUAL "long_identifier")
  set(head "function main():\n    ")
  set(tail "()\n")
  string(LENGTH "${head}${tail}" fixed_length)
  math(EXPR identifier_length "${SIZE} - ${fixed_length}")
  string(REPEAT "a" ${identifier_length} identifier)
  file(WRITE "${main_file}" "${head}${identifier}${tail}")
  check_generated_size()
elseif(MODE STREQUAL "call_chain")
  # main (depth 1) -> f2 -> ... -> fDEPTH, which returns 7. fK (K >= 2) is declared on line
  # 2K - 1 and calls fK+1 on line 2K, column 12; main calls f2 on line 2, column 12.
  set(source "function main():\n    return f2()\n")
  math(EXPR last_caller "${DEPTH} - 1")
  if(last_caller GREATER_EQUAL 2)
    foreach(index RANGE 2 ${last_caller})
      math(EXPR next "${index} + 1")
      string(APPEND source "function f${index}(): Integer:\n    return f${next}()\n")
    endforeach()
  endif()
  string(APPEND source "function f${DEPTH}(): Integer:\n    return 7\n")
  file(WRITE "${main_file}" "${source}")
elseif(MODE STREQUAL "eval_depth")
  # T-SEC-4 (D24): f's body is level 1, so `return g(g(...g(f(x))...))` with 254 calls to g puts
  # f's argument list, the innermost, at nesting level 256, the parser limit.
  string(REPEAT "g(" 254 opens)
  string(REPEAT ")" 254 closes)
  file(WRITE "${main_file}"
       "function main():\n    return f(1)\n"
       "function f(x: Integer): Integer:\n    return ${opens}f(x)${closes}\n"
       "function g(x: Integer): Integer:\n    return x\n")
elseif(MODE STREQUAL "pseudo_nesting")
  # `main`'s body is level 1 and `echo(`'s argument list is level 2, so NESTING - 2 nested
  # `typeof(` put the innermost argument list at level NESTING. `closes` balances the `typeof(`s;
  # the template's literal `)` after it closes `echo(`.
  math(EXPR typeof_count "${NESTING} - 2")
  string(REPEAT "typeof(" ${typeof_count} opens)
  string(REPEAT ")" ${typeof_count} closes)
  file(WRITE "${main_file}" "function main():\n    echo(${opens}\"x\"${closes})\n")
elseif(MODE STREQUAL "broken_pipe_program")
  string(REPEAT "x" 65536 xs)
  file(WRITE "${main_file}"
       "function spam(s: String): null:\n"
       "    echo(s)\n"
       "    spam(s)\n"
       "function main():\n"
       "    spam(\"${xs}\")\n")
elseif(MODE STREQUAL "copy_folder")
  # FIXTURE_DIR was checked above. Copy each regular file byte for byte; nothing is written to the
  # source directory.
  file(GLOB fixture_entries LIST_DIRECTORIES false "${FIXTURE_DIR}/*")
  if(fixture_entries STREQUAL "")
    message(FATAL_ERROR "run_case.cmake: FIXTURE_DIR '${FIXTURE_DIR}' holds no files")
  endif()
  foreach(fixture_entry IN LISTS fixture_entries)
    cmake_path(GET fixture_entry FILENAME fixture_name)
    file(COPY_FILE "${fixture_entry}" "${WORK_DIR}/${fixture_name}")
  endforeach()
else()
  message(FATAL_ERROR "run_case.cmake: unknown MODE '${MODE}'")
endif()

set(run_timeout 10)
if(MODE STREQUAL "long_line")
  set(run_timeout 20)
endif()

set(stdout_file "${WORK_DIR}/stdout.bin")
if(STDOUT_MODE STREQUAL "capture")
  execute_process(
    COMMAND "${PHILIOS_BIN}" ${ARGS}
    WORKING_DIRECTORY "${WORK_DIR}"
    TIMEOUT ${run_timeout}
    RESULT_VARIABLE status
    OUTPUT_FILE "${stdout_file}"
    ERROR_VARIABLE stderr_text)
elseif(STDOUT_MODE STREQUAL "dev_full")
  execute_process(
    COMMAND "${PHILIOS_BIN}" ${ARGS}
    WORKING_DIRECTORY "${WORK_DIR}"
    TIMEOUT ${run_timeout}
    RESULT_VARIABLE status
    OUTPUT_FILE "/dev/full"
    ERROR_VARIABLE stderr_text)
elseif(STDOUT_MODE STREQUAL "closed")
  execute_process(
    COMMAND sh -c "exec \"$0\" \"$@\" >&-" "${PHILIOS_BIN}" ${ARGS}
    WORKING_DIRECTORY "${WORK_DIR}"
    TIMEOUT ${run_timeout}
    RESULT_VARIABLE status
    ERROR_VARIABLE stderr_text)
elseif(STDOUT_MODE STREQUAL "broken_pipe")
  execute_process(
    COMMAND "${PHILIOS_BIN}" ${ARGS}
    COMMAND "${CMAKE_COMMAND}" -E true
    WORKING_DIRECTORY "${WORK_DIR}"
    TIMEOUT ${run_timeout}
    RESULTS_VARIABLE pipeline_statuses
    RESULT_VARIABLE pipeline_status
    ERROR_VARIABLE stderr_text)
  list(LENGTH pipeline_statuses pipeline_status_count)
  if(pipeline_status_count LESS 1)
    set(status "${pipeline_status}")
  else()
    list(GET pipeline_statuses 0 status)
  endif()
else()
  message(FATAL_ERROR "run_case.cmake: unknown STDOUT_MODE '${STDOUT_MODE}'")
endif()

if(MODE STREQUAL "unreadable")
  execute_process(COMMAND chmod 600 "${main_file}")
endif()

set(failures "")

if(STDOUT_MODE STREQUAL "capture")
  if(EXISTS "${stdout_file}")
    file(READ "${stdout_file}" stdout_hex HEX)
  else()
    set(stdout_hex "")
  endif()

  if(DEFINED EXPECT_STDOUT_HEX)
    if(NOT stdout_hex STREQUAL "${EXPECT_STDOUT_HEX}")
      string(APPEND failures "expected stdout hex ${EXPECT_STDOUT_HEX}, got ${stdout_hex}\n")
    endif()
  elseif(NOT stdout_hex STREQUAL "")
    string(APPEND failures "stdout is not empty\n")
  endif()
endif()

if(EXPECT_NO_CRASH)
  if(NOT status MATCHES "^(0|65|66|70)$")
    string(APPEND failures "expected a normal interpreter status (0, 65, 66, or 70), got '${status}'\n")
  endif()
  string(FIND "${stderr_text}" "Sanitizer" sanitizer_at)
  if(NOT sanitizer_at EQUAL -1)
    string(APPEND failures "sanitizer report on stderr\n")
  endif()
else()
  if(NOT DEFINED EXPECT_STATUS)
    message(FATAL_ERROR "run_case.cmake: EXPECT_STATUS is required")
  endif()
  if(NOT status STREQUAL "${EXPECT_STATUS}")
    string(APPEND failures "expected exit status ${EXPECT_STATUS}, got '${status}'\n")
  endif()

  if(EXPECT_KIND STREQUAL "empty")
    if(NOT stderr_text STREQUAL "")
      string(APPEND failures "expected empty stderr\n")
    endif()
  elseif(EXPECT_KIND STREQUAL "located")
    if(NOT DEFINED EXPECT_CLASS OR NOT EXPECT_LINE MATCHES "^[1-9][0-9]*$")
      message(FATAL_ERROR "run_case.cmake: EXPECT_KIND=located needs EXPECT_CLASS and EXPECT_LINE")
    endif()
    if(DEFINED EXPECT_COLUMN AND NOT EXPECT_COLUMN STREQUAL "")
      set(prefix "main.phil:${EXPECT_LINE}:${EXPECT_COLUMN}: ${EXPECT_CLASS} error: ")
      string(FIND "${stderr_text}" "${prefix}" prefix_at)
      if(NOT prefix_at EQUAL 0)
        string(APPEND failures "expected stderr to start with '${prefix}'\n")
      endif()
    else()
      # Line-only check, used only where the plan leaves the column open (see the case's comment).
      if(NOT stderr_text MATCHES "^main\\.phil:${EXPECT_LINE}:[1-9][0-9]*: ${EXPECT_CLASS} error: ")
        string(APPEND failures
               "expected stderr to start with 'main.phil:${EXPECT_LINE}:<column>: ${EXPECT_CLASS} error: '\n")
      endif()
    endif()
  elseif(EXPECT_KIND STREQUAL "startup")
    string(FIND "${stderr_text}" "philios: startup error: main.phil: " prefix_at)
    if(NOT prefix_at EQUAL 0)
      string(APPEND failures "expected a startup error naming main.phil\n")
    endif()
  elseif(EXPECT_KIND STREQUAL "usage")
    if(NOT stderr_text STREQUAL "philios: usage error: philios takes no arguments\nusage: philios\n")
      string(APPEND failures "expected exactly the usage message\n")
    endif()
  else()
    message(FATAL_ERROR "run_case.cmake: unknown EXPECT_KIND '${EXPECT_KIND}'")
  endif()

  # One diagnostic per failure (§4): a located or startup error is exactly one line.
  if(EXPECT_KIND STREQUAL "located" OR EXPECT_KIND STREQUAL "startup")
    string(REGEX MATCHALL "\n" newlines "${stderr_text}")
    list(LENGTH newlines newline_count)
    if(NOT newline_count EQUAL 1 OR NOT stderr_text MATCHES "\n$")
      string(APPEND failures "expected exactly one diagnostic line on stderr\n")
    endif()
  endif()

  if(DEFINED EXPECT_MESSAGE AND NOT EXPECT_MESSAGE STREQUAL "")
    string(FIND "${stderr_text}" "${EXPECT_MESSAGE}" message_at)
    if(message_at EQUAL -1)
      string(APPEND failures "expected the diagnostic to contain '${EXPECT_MESSAGE}'\n")
    endif()
  endif()
  if(DEFINED FORBID_MESSAGE AND NOT FORBID_MESSAGE STREQUAL "")
    string(FIND "${stderr_text}" "${FORBID_MESSAGE}" forbidden_at)
    if(NOT forbidden_at EQUAL -1)
      string(APPEND failures "the diagnostic must not contain '${FORBID_MESSAGE}'\n")
    endif()
  endif()
endif()

if(NOT failures STREQUAL "")
  string(LENGTH "${stderr_text}" stderr_length)
  if(stderr_length GREATER 2000)
    string(SUBSTRING "${stderr_text}" 0 2000 stderr_text)
    string(APPEND stderr_text "...")
  endif()
  message(FATAL_ERROR
          "CLI case failed (mode ${MODE}):\n${failures}status: ${status}\nstderr: [${stderr_text}]")
endif()

# Remove large generated inputs; small cases stay in the build tree for inspection.
if(MODE MATCHES "^(sized|deep_open|deep_calls|long_line|long_identifier|nesting|call_chain|eval_depth)$")
  file(REMOVE_RECURSE "${WORK_DIR}")
endif()
