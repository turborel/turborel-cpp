# R-STYLE-1: the C++ sources are formatted as .clang-format says. Run as the
# test `check.format`, or with -DFIX=ON by the target `format`, which
# formats them in place:
#   cmake -DROOT=<source dir> -DCLANG_FORMAT=<clang-format> [-DFIX=ON] -P cmake/CheckFormat.cmake
cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED ROOT OR NOT DEFINED CLANG_FORMAT)
  message(FATAL_ERROR "usage: cmake -DROOT=<source dir> -DCLANG_FORMAT=<clang-format> [-DFIX=ON] -P CheckFormat.cmake")
endif()

file(GLOB_RECURSE files "${ROOT}/include/*.hpp" "${ROOT}/src/*.cpp" "${ROOT}/src/*.hpp" "${ROOT}/test/*.cpp"
     "${ROOT}/test/*.hpp")

if(FIX)
  set(mode -i)
else()
  set(mode --dry-run --Werror)
endif()

execute_process(
  COMMAND "${CLANG_FORMAT}" ${mode} ${files}
  WORKING_DIRECTORY "${ROOT}"
  RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "R-STYLE-1: the files above are not formatted; build the target `format` to format them")
endif()
