# The manual's rules and the documents' links, run as the test
# `check.manual`:
#   cmake -DROOT=<source dir> -P cmake/CheckManual.cmake
# R-DOC-7 and RULES.md D4: every page of doc/ opens with its `# Title` and
# a line holding only `[Contents](README.md)`, as turborel.com's
# documentation site reads it; each volume's README.md lists its pages,
# and doc/README.md each volume; every relative link of a document names a
# file that exists, and a link of doc/ that leaves doc/ is a full URL.
cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED ROOT)
  message(FATAL_ERROR "usage: cmake -DROOT=<source dir> -P CheckManual.cmake")
endif()

# The line a page holds under its title, which the site removes.
set(contents_line "[Contents](README.md)")

set(failures "")

# Notes a failure of `rule` at `file`.
macro(fail rule file what)
  file(RELATIVE_PATH where "${ROOT}" "${file}")
  list(APPEND failures "${where}: ${rule}: ${what}")
endmacro()

# The relative links of `text` into `out`: what each `](...)` names, less
# its anchor, but full URLs and anchors alone. Square brackets group a
# CMake list's items, so they are replaced first.
function(links_of text out)
  string(REPLACE ";" "@SC@" text "${text}")
  string(REPLACE "[" "@LB@" text "${text}")
  string(REPLACE "]" "@RB@" text "${text}")
  string(REGEX MATCHALL "@RB@\\([^)# ]+" found "${text}")
  set(targets "")
  foreach(link IN LISTS found)
    string(SUBSTRING "${link}" 5 -1 target)
    if(NOT target MATCHES "^[a-z]+:")
      list(APPEND targets "${target}")
    endif()
  endforeach()
  set(${out} "${targets}" PARENT_SCOPE)
endfunction()

file(GLOB_RECURSE documents "${ROOT}/*.md")
list(FILTER documents EXCLUDE REGEX "/(build|\\.git|\\.claude)/")
foreach(file IN LISTS documents)
  file(READ "${file}" text)
  get_filename_component(dir "${file}" DIRECTORY)
  links_of("${text}" targets)
  foreach(target IN LISTS targets)
    get_filename_component(resolved "${dir}/${target}" ABSOLUTE)
    if(NOT EXISTS "${resolved}")
      fail(R-DOC-7 "${file}" "a link to ${target}, which does not exist")
    elseif(file MATCHES "^${ROOT}/doc/" AND NOT resolved MATCHES "^${ROOT}/doc/")
      fail(R-DOC-7 "${file}" "a link to ${target}, out of doc/, which is not a full URL")
    endif()
  endforeach()
endforeach()

file(GLOB_RECURSE pages "${ROOT}/doc/*.md")
foreach(page IN LISTS pages)
  get_filename_component(name "${page}" NAME)
  if(name STREQUAL "README.md")
    continue()
  endif()
  file(READ "${page}" text)
  if(NOT text MATCHES "^# [^\n ][^\n]*\n\n?\\[Contents\\]\\(README\\.md\\)\n")
    fail(R-DOC-7 "${page}" "does not open with its `# Title`, then a line holding only `${contents_line}`")
  endif()
endforeach()

# Each README.md of doc/ lists what is beside it: the pages of its folder,
# and the volumes under it.
file(GLOB_RECURSE readmes "${ROOT}/doc/README.md" "${ROOT}/doc/*/README.md")
foreach(readme IN LISTS readmes)
  file(READ "${readme}" text)
  get_filename_component(dir "${readme}" DIRECTORY)
  links_of("${text}" targets)
  file(GLOB beside RELATIVE "${dir}" "${dir}/*.md" "${dir}/*/README.md")
  list(REMOVE_ITEM beside "README.md")
  foreach(listed IN LISTS beside)
    if(NOT listed IN_LIST targets)
      fail(R-DOC-7 "${readme}" "does not list ${listed}")
    endif()
  endforeach()
endforeach()

if(failures)
  list(JOIN failures "\n" report)
  message(FATAL_ERROR "${report}")
endif()
message(STATUS "The manual's rules hold")
