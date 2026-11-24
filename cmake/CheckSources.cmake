# The source rules a tool can check, run as the test `check.sources`:
#   cmake -DROOT=<source dir> -P cmake/CheckSources.cmake
# R-SIZE-2 (files of at most 600 lines), R-DOC-6 (a file doc first), R-DOC-5
# (no open-work comments, and code never cites the open-work list),
# R-TRACE-1 (every test cites what it verifies, by a tag of RULES.md D6),
# R-TRACE-2 (every heading, decision and rule cited exists), R-LINT-3
# (every NOLINT names its check and says why) and R-PROC-1 (no merge
# conflict marker in a source or document). The manual's rules are
# cmake/CheckManual.cmake's.
cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED ROOT)
  message(FATAL_ERROR "usage: cmake -DROOT=<source dir> -P CheckSources.cmake")
endif()

# R-SIZE-2: the most lines a source file has.
set(max_lines 600)

# RULES.md D6: the headings a citation may name, those of Turborel's
# documents at the commit CONFORMANCE.md names, 5863fe65, their titles
# among them. `[PROTOCOL <heading>]` names one of The Native Protocol,
# doc/client/05-protocol.md, where Lob 'L' is printed twice.
set(protocol_headings
    "The Native Protocol" "Frames" "Compressed frames" "Values and fields" "Data types" "Values"
    "JSON values" "Composite fields" "The tail" "Opening a connection" "Mechanisms" "Capabilities"
    "Requests" "Hello 'H'" "Answer 'A'" "Query 'Q'" "Prepare 'P'" "Execute 'E'" "Fetch 'F'" "Close 'C'"
    "Deallocate 'D'" "Lob 'L'" "Cancel 'Z'" "Terminate 'X'" "Describe 'Y'" "Ping 'G'" "Reset 'W'"
    "ChangeUser 'U'" "CopyIn 'I'" "CopyData 'J'" "CopyEnd 'N'" "Debug 'V'" "Replies" "Challenge 'C'"
    "Ready 'R'" "Prepared 'P'" "Parameter modes" "Description 'D'" "Declared types" "Rows 'B'" "Suspended 'S'"
    "Complete 'K'" "Error 'E'" "Notice 'N'" "Status 'T'" "Pong 'G'" "Debugged 'V'" "Event 'W'" "Conversations"
    "Errors and notices"
    "Sample frames" "A worked example")

# `[CLIENT <heading>]` names one of The Client Library,
# doc/client/02-library.md, or of Settings, Pipelines and Pools,
# doc/client/02-library-connections.md, which share none.
set(client_headings
    "The Client Library" "Connecting" "Timeouts" "Running statements" "Prepared statements" "Large objects"
    "XML values" "UUIDs" "Importing" "What a connection asks for" "What the server tells"
    "Controlling the session" "Debugging routines" "Copying and exporting" "The SQL-agent" "Settings, Pipelines and Pools"
    "Connecting by settings" "Where settings come from" "The connection URI" "The service file"
    "The password file" "Several servers" "TLS by mode" "Pipelines" "Pools" "The SCRAM key cache")

# Spelt in parts, so this file does not break its own rules.
string(CONCAT open_work "TO" "DO")
string(CONCAT fix_me "FIX" "ME")
string(REPEAT "<" 7 conflict_start)
string(REPEAT "=" 7 conflict_middle)
string(REPEAT ">" 7 conflict_end)

file(GLOB_RECURSE cpp_files "${ROOT}/include/*.hpp" "${ROOT}/src/*.cpp" "${ROOT}/src/*.hpp" "${ROOT}/test/*.cpp"
     "${ROOT}/test/*.hpp")
file(GLOB_RECURSE cmake_files "${ROOT}/cmake/*" "${ROOT}/test/CMakeLists.txt" "${ROOT}/test/package/CMakeLists.txt")
list(APPEND cmake_files "${ROOT}/CMakeLists.txt" "${ROOT}/CMakePresets.json")
file(GLOB_RECURSE text_files "${ROOT}/*.md" "${ROOT}/.github/*" "${ROOT}/.githooks/*" "${ROOT}/tools/*")
list(FILTER text_files EXCLUDE REGEX "/(build|\\.git|\\.claude)/")
file(READ "${ROOT}/RULES.md" rules)

set(failures "")

# Notes a failure of `rule` at `file`.
macro(fail rule file what)
  file(RELATIVE_PATH where "${ROOT}" "${file}")
  list(APPEND failures "${where}: ${rule}: ${what}")
endmacro()

# R-TRACE-2: fails each citation in `listed`, text whose semicolons and
# square brackets are replaced, of a heading, decision or rule that does
# not exist: `[PROTOCOL <heading>]`, `[CLIENT <heading>]`, `[DESIGN] D<n>`,
# `RULES.md D<n>` but Turborel's own, and `R-<family>-<n>`.
function(check_citations file listed)
  set(failures "${failures}")
  # A citation may run on into the next line of its comment.
  string(REGEX REPLACE "\n[ \t]*(///?|#)[ \t]*" " " listed "${listed}")
  string(REGEX MATCHALL "@LB@PROTOCOL ([^@\n]|@[^R])+@RB@" protocol "${listed}")
  foreach(cited IN LISTS protocol)
    string(REGEX REPLACE "^@LB@PROTOCOL (.+)@RB@$" "\\1" heading "${cited}")
    if(NOT heading IN_LIST protocol_headings)
      fail(R-TRACE-2 "${file}" "[PROTOCOL ${heading}] is not a heading of The Native Protocol")
    endif()
  endforeach()
  string(REGEX MATCHALL "@LB@CLIENT ([^@\n]|@[^R])+@RB@" client "${listed}")
  foreach(cited IN LISTS client)
    string(REGEX REPLACE "^@LB@CLIENT (.+)@RB@$" "\\1" heading "${cited}")
    if(NOT heading IN_LIST client_headings)
      fail(R-TRACE-2 "${file}" "[CLIENT ${heading}] is not a heading of the client library's chapters")
    endif()
  endforeach()
  string(REGEX MATCHALL "(@LB@DESIGN@RB@|[Tt]urborel's RULES\\.md|RULES\\.md) D[0-9]+" decisions "${listed}")
  foreach(cited IN LISTS decisions)
    string(REGEX MATCH "D[0-9]+$" decision "${cited}")
    string(FIND "${rules}" "\n#### ${decision}. " at)
    if(at EQUAL -1 AND NOT cited MATCHES "^[Tt]urborel's")
      fail(R-TRACE-2 "${file}" "${decision} is not a decision of RULES.md")
    endif()
  endforeach()
  string(REGEX MATCHALL "R-[A-Z]+-[0-9]+" rule_ids "${listed}")
  foreach(rule IN LISTS rule_ids)
    string(FIND "${rules}" "| ${rule} |" at)
    if(at EQUAL -1)
      fail(R-TRACE-2 "${file}" "${rule} is not a rule of RULES.md")
    endif()
  endforeach()
  set(failures "${failures}" PARENT_SCOPE)
endfunction()

foreach(file IN LISTS cpp_files cmake_files text_files)
  file(READ "${file}" text)
  foreach(marker IN ITEMS "\n${conflict_start} " "\n${conflict_middle}\n" "\n${conflict_end} ")
    string(FIND "${text}" "${marker}" at)
    if(at GREATER -1)
      fail(R-PROC-1 "${file}" "a merge conflict marker")
    endif()
  endforeach()
endforeach()

foreach(file IN LISTS cpp_files cmake_files)
  file(READ "${file}" text)
  if(text MATCHES "${open_work}|${fix_me}")
    fail(R-DOC-5 "${file}" "an open-work note, or a citation of the list open work goes in")
  endif()
endforeach()

foreach(file IN LISTS cpp_files)
  file(READ "${file}" text)
  string(REGEX MATCHALL "\n" newlines "${text}")
  list(LENGTH newlines lines)
  if(lines GREATER max_lines)
    fail(R-SIZE-2 "${file}" "${lines} lines, more than ${max_lines}")
  endif()
  string(REGEX MATCHALL "NOLINT[A-Z]*[^\n]*" silenced "${text}")
  foreach(nolint IN LISTS silenced)
    if(NOT nolint MATCHES "^NOLINT(NEXTLINE)?\\([a-z0-9.,-]+\\): [^ ]")
      fail(R-LINT-3 "${file}" "a NOLINT that does not name its check and then say why: ${nolint}")
    endif()
  endforeach()
  string(FIND "${text}" "/// @file" at)
  if(NOT at EQUAL 0)
    fail(R-DOC-6 "${file}" "does not begin with a `/// @file` doc")
  endif()
  # Semicolons and square brackets would split or group the matches as a
  # CMake list, so they are replaced before the tests are listed.
  string(REPLACE ";" "@SC@" listed "${text}")
  string(REPLACE "[" "@LB@" listed "${listed}")
  string(REPLACE "]" "@RB@" listed "${listed}")
  string(REGEX MATCHALL "\nTEST_CASE\\(" tests "${listed}")
  string(REGEX MATCHALL "\n/// Verifies: @LB@[^\n]*\n(///[^\n]*\n)*TEST_CASE\\(" cited "${listed}")
  list(LENGTH tests test_count)
  list(LENGTH cited cited_count)
  if(NOT test_count EQUAL cited_count)
    fail(R-TRACE-1 "${file}" "a TEST_CASE without a `/// Verifies:` paragraph right above it")
  endif()
  foreach(test IN LISTS cited)
    if(NOT test MATCHES "Verifies: @LB@(PROTOCOL |CLIENT |DESIGN@RB@ D[0-9]|TOOL@RB@ R-)")
      fail(R-TRACE-1 "${file}" "a `/// Verifies:` paragraph that starts with no tag RULES.md D6 takes")
    endif()
  endforeach()
  check_citations("${file}" "${listed}")
endforeach()

# This file spells the citations' forms out, and cites nothing.
list(FILTER cmake_files EXCLUDE REGEX "/CheckSources\\.cmake$")
foreach(file IN LISTS cmake_files)
  file(READ "${file}" text)
  string(REPLACE ";" "@SC@" listed "${text}")
  string(REPLACE "[" "@LB@" listed "${listed}")
  string(REPLACE "]" "@RB@" listed "${listed}")
  check_citations("${file}" "${listed}")
endforeach()

if(failures)
  list(JOIN failures "\n" report)
  message(FATAL_ERROR "${report}")
endif()
message(STATUS "The source rules hold")
