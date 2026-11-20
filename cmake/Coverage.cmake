# R-COV-1 and R-COV-2: merges the profiles the tests wrote, reports the
# coverage of the library's own sources, and fails below the floors of
# cmake/CoverageFloor.cmake. Run by the target `coverage-report`:
#   cmake -DPROFILES=<dir> -DBINARIES=<binary;...> -DROOT=<source dir> [-DLLVM_MAJOR=<n>] -P cmake/Coverage.cmake
cmake_minimum_required(VERSION 3.25)

include("${CMAKE_CURRENT_LIST_DIR}/CoverageFloor.cmake")

foreach(var IN ITEMS PROFILES BINARIES ROOT)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "usage: cmake -DPROFILES=<dir> -DBINARIES=<binary;...> -DROOT=<source dir> -P Coverage.cmake")
  endif()
endforeach()

# Finds an LLVM tool into `out`: by its versioned name, its plain one, or
# from Xcode's tools.
function(find_llvm_tool out tool)
  find_program(found NAMES ${tool}-${LLVM_MAJOR} ${tool} NO_CACHE)
  if(NOT found AND APPLE)
    execute_process(
      COMMAND xcrun --find ${tool}
      OUTPUT_VARIABLE found
      OUTPUT_STRIP_TRAILING_WHITESPACE)
  endif()
  if(NOT found)
    message(FATAL_ERROR "[DESIGN] RULES.md D10: ${tool} not found, of LLVM ${LLVM_MAJOR} or unversioned")
  endif()
  set(${out} "${found}" PARENT_SCOPE)
endfunction()

find_llvm_tool(profdata llvm-profdata)
find_llvm_tool(cov llvm-cov)

file(GLOB raw "${PROFILES}/*.profraw")
if(NOT raw)
  message(FATAL_ERROR "R-COV-1: no profiles in ${PROFILES}; run the tests of the coverage preset first")
endif()
set(merged "${PROFILES}/merged.profdata")
execute_process(COMMAND "${profdata}" merge -sparse ${raw} -o "${merged}" COMMAND_ERROR_IS_FATAL ANY)

# The library's own sources alone: not the tests, nor what was fetched.
list(POP_FRONT BINARIES first)
set(objects "")
foreach(binary IN LISTS BINARIES)
  list(APPEND objects "-object=${binary}")
endforeach()
set(scope "-instr-profile=${merged}" "-ignore-filename-regex=(_deps|[/\\\\]test[/\\\\])" "${first}" ${objects})

execute_process(COMMAND "${cov}" report ${scope} COMMAND_ERROR_IS_FATAL ANY)
execute_process(
  COMMAND "${cov}" export -summary-only ${scope}
  OUTPUT_VARIABLE summary COMMAND_ERROR_IS_FATAL ANY)

set(below "")
foreach(measure IN ITEMS lines functions regions)
  string(JSON percent GET "${summary}" data 0 totals ${measure} percent)
  string(TOUPPER "${measure}" name)
  set(floor "${TURBOREL_CPP_COVERAGE_FLOOR_${name}}")
  message(STATUS "${measure}: ${percent}% covered, floor ${floor}%")
  if(percent LESS floor)
    list(APPEND below "${measure} ${percent}% is below its floor of ${floor}%")
  endif()
endforeach()
if(below)
  list(JOIN below "; " report)
  message(FATAL_ERROR "R-COV-2: ${report}")
endif()
