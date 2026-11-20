# The source rules a tool can check, run as the test `check.sources`:
#   cmake -DROOT=<source dir> -P cmake/CheckSources.cmake
# R-SIZE-2 (files of at most 600 lines), R-DOC-6 (a file doc first), R-DOC-7
# (every relative link of a document resolves), R-DOC-5
# (no open-work comments, and code never cites the open-work list), R-TRACE-1
# (every test cites what it verifies, by a heading that exists), R-DEV-1
# (every NOLINT names its check and says why) and R-PROC-1 (no merge
# conflict marker in a source or document).
cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED ROOT)
  message(FATAL_ERROR "usage: cmake -DROOT=<source dir> -P CheckSources.cmake")
endif()

# R-SIZE-2: the most lines a source file has.
set(max_lines 600)

# The headings of doc/client/05-protocol.md, protocol version 1, that a
# test may cite as [PROTOCOL <heading>].
set(protocol_headings
    "Frames" "Compressed frames" "Values and fields" "Data types" "Values" "Composite fields" "The tail"
    "Opening a connection" "Mechanisms" "Capabilities" "Requests" "Hello 'H'" "Answer 'A'" "Query 'Q'"
    "Prepare 'P'" "Execute 'E'" "Fetch 'F'" "Close 'C'" "Deallocate 'D'" "Lob 'L'" "Cancel 'Z'" "Terminate 'X'"
    "Describe 'Y'" "Ping 'G'" "Reset 'W'" "ChangeUser 'U'" "CopyIn 'I'" "CopyData 'J'" "CopyEnd 'N'" "Replies"
    "Challenge 'C'" "Ready 'R'" "Prepared 'P'" "Description 'D'" "Rows 'B'" "Suspended 'S'" "Complete 'K'"
    "Error 'E'" "Notice 'N'" "Status 'T'" "Pong 'G'" "Conversations" "Errors and notices" "A worked example")

# The headings of doc/client/02-library.md and 02-library-connections.md
# that a test may cite as [CLIENT <heading>].
set(client_headings
    "Connecting" "Timeouts" "Running statements" "Prepared statements" "Large objects" "XML values" "UUIDs"
    "Importing" "What a connection asks for" "What the server tells" "Controlling the session"
    "Copying and exporting" "The SQL-agent" "Connecting by settings" "Where settings come from"
    "The connection URI" "The service file" "The password file" "Several servers" "TLS by mode" "Pipelines"
    "Pools" "The SCRAM key cache")

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

foreach(file IN LISTS cpp_files cmake_files text_files)
  file(READ "${file}" text)
  foreach(marker IN ITEMS "\n${conflict_start} " "\n${conflict_middle}\n" "\n${conflict_end} ")
    string(FIND "${text}" "${marker}" at)
    if(at GREATER -1)
      fail(R-PROC-1 "${file}" "a merge conflict marker")
    endif()
  endforeach()
endforeach()

# R-DOC-7: every relative link of a document names a file that exists.
file(GLOB_RECURSE doc_files "${ROOT}/*.md")
list(FILTER doc_files EXCLUDE REGEX "/(build|\\.git|\\.claude)/")
foreach(file IN LISTS doc_files)
  file(READ "${file}" text)
  get_filename_component(dir "${file}" DIRECTORY)
  # Square brackets group the items of a CMake list, so they are replaced
  # before the links are listed.
  string(REPLACE "[" "@LB@" text "${text}")
  string(REPLACE "]" "@RB@" text "${text}")
  string(REGEX MATCHALL "@RB@\\([^)# ;]+" links "${text}")
  foreach(link IN LISTS links)
    string(SUBSTRING "${link}" 5 -1 target)
    if(target MATCHES "^[a-z]+:")
      continue()
    endif()
    if(NOT EXISTS "${dir}/${target}")
      fail(R-DOC-7 "${file}" "a link to ${target}, which does not exist")
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
      fail(R-DEV-1 "${file}" "a NOLINT that does not name its check and then say why: ${nolint}")
    endif()
  endforeach()
  string(FIND "${text}" "/// @file" at)
  if(NOT at EQUAL 0)
    fail(R-DOC-6 "${file}" "does not begin with a `/// @file` doc")
  endif()
  string(REGEX MATCHALL "\nTEST_CASE\\(" tests "${text}")
  string(REGEX MATCHALL "\n/// Verifies: \\[[^\n]*\n(///[^\n]*\n)*TEST_CASE\\(" cited "${text}")
  list(LENGTH tests test_count)
  list(LENGTH cited cited_count)
  if(NOT test_count EQUAL cited_count)
    fail(R-TRACE-1 "${file}" "a TEST_CASE without a `/// Verifies:` paragraph right above it")
  endif()
  foreach(test IN LISTS cited)
    if(test MATCHES "Verifies: \\[PROTOCOL ([^]\n]+)\\]")
      if(NOT CMAKE_MATCH_1 IN_LIST protocol_headings)
        fail(R-TRACE-1 "${file}" "[PROTOCOL ${CMAKE_MATCH_1}] is not a heading of 05-protocol.md")
      endif()
    elseif(test MATCHES "Verifies: \\[CLIENT ([^]\n]+)\\]")
      if(NOT CMAKE_MATCH_1 IN_LIST client_headings)
        fail(R-TRACE-1 "${file}" "[CLIENT ${CMAKE_MATCH_1}] is not a heading of the client library's chapters")
      endif()
    elseif(test MATCHES "Verifies: \\[DESIGN\\] (D[0-9]+)")
      string(FIND "${rules}" "\n#### ${CMAKE_MATCH_1}. " at)
      if(at EQUAL -1)
        fail(R-TRACE-1 "${file}" "[DESIGN] ${CMAKE_MATCH_1} is not a decision in RULES.md")
      endif()
    else()
      fail(R-TRACE-1 "${file}" "a `/// Verifies:` paragraph that starts with no tag R-TRACE-1 takes")
    endif()
  endforeach()
endforeach()

if(failures)
  list(JOIN failures "\n" report)
  message(FATAL_ERROR "${report}")
endif()
message(STATUS "The source rules hold")
