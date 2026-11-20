# The manual's rules and the documents' links, run as the test
# `check.manual`:
#   cmake -DROOT=<source dir> -P cmake/CheckManual.cmake
# R-DOC-7 and RULES.md D4: every page of doc/ opens with its `# Title` and
# a line holding only `[Contents](README.md)`, as turborel.com's
# documentation site reads it; each volume's README.md lists its pages,
# and doc/README.md each volume; user/01-installing.md is there; doc/ holds
# no images; every relative link of a document names a file that exists;
# and doc/ links to this repository's files by relative links, which the
# site points at GitHub, and to Turborel's manual by its GitHub address.
cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED ROOT)
  message(FATAL_ERROR "usage: cmake -DROOT=<source dir> -P CheckManual.cmake")
endif()

# The line a page holds under its title, which the site removes.
set(contents_line "[Contents](README.md)")
# This repository on GitHub, which doc/ links to by relative links alone.
set(own_repository "https://github.com/turborel/turborel-cpp")
# Turborel's repository, which doc/ links to for its manual alone.
set(turborel_repository "https://github.com/turborel/turborel/")
set(turborel_manual "https://github.com/turborel/turborel/blob/main/doc/")

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
    endif()
  endforeach()
endforeach()

file(GLOB_RECURSE pages "${ROOT}/doc/*")
foreach(page IN LISTS pages)
  if(NOT page MATCHES "\\.md$")
    fail(R-DOC-7 "${page}" "is not a page: doc/ holds pages alone, and no images")
    continue()
  endif()
  file(READ "${page}" text)
  string(REPLACE "[" "@LB@" text "${text}")
  string(REPLACE "]" "@RB@" text "${text}")
  if(text MATCHES "!@LB@")
    fail(R-DOC-7 "${page}" "shows an image, and doc/ holds none")
  endif()
  string(REGEX MATCHALL "@RB@\\(https://github\\.com/turborel/[^)]+" urls "${text}")
  foreach(url IN LISTS urls)
    string(SUBSTRING "${url}" 5 -1 url)
    string(FIND "${url}" "${own_repository}" own)
    string(FIND "${url}" "${turborel_repository}" turborel)
    string(FIND "${url}" "${turborel_manual}" manual)
    if(own EQUAL 0)
      fail(R-DOC-7 "${page}" "links to ${url}, a file of this repository, by its address, not a relative link")
    elseif(turborel EQUAL 0 AND NOT manual EQUAL 0)
      fail(R-DOC-7 "${page}" "links to ${url}, of Turborel's repository but not its manual")
    endif()
  endforeach()
endforeach()
if(NOT EXISTS "${ROOT}/doc/user/01-installing.md")
  fail(R-DOC-7 "${ROOT}/doc/user" "has no 01-installing.md, whose name the site keeps")
endif()

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
