# CLI golden-test driver (plan 0001 §7, §9). Runs the real `philios` binary in a fresh working
# directory with execute_process(WORKING_DIRECTORY ... TIMEOUT 10) and checks the exit status, the
# stderr text (a class/line/column prefix, the usage text, a startup prefix, or empty on success),
# and that stdout is empty. Large and structural inputs are generated here, at test time, into the
# build directory; they are never committed.
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
#   FIXTURE          Source fixture for the file-based modes.
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
#   SKIP_LABEL       Test label used in the skip line `<SKIP_LABEL> SKIPPED: <reason>`.

cmake_minimum_required(VERSION 3.28)

foreach(required PHILIOS_BIN WORK_DIR MODE)
  if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
    message(FATAL_ERROR "run_case.cmake: ${required} is required")
  endif()
endforeach()

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
else()
  message(FATAL_ERROR "run_case.cmake: unknown MODE '${MODE}'")
endif()

execute_process(
  COMMAND "${PHILIOS_BIN}" ${ARGS}
  WORKING_DIRECTORY "${WORK_DIR}"
  TIMEOUT 10
  RESULT_VARIABLE status
  OUTPUT_VARIABLE stdout_text
  ERROR_VARIABLE stderr_text)

if(MODE STREQUAL "unreadable")
  execute_process(COMMAND chmod 600 "${main_file}")
endif()

set(failures "")

if(NOT stdout_text STREQUAL "")
  string(APPEND failures "stdout is not empty\n")
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
