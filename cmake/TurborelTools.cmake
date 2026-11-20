# The checks beside the compiler (RULES.md D8): clang-tidy as each of the
# project's targets builds, and, as tests CTest runs, clang-format, the
# source rules and Doxygen. The tools are pinned to one LLVM major version,
# since another version formats and lints differently.
include_guard(GLOBAL)

# The LLVM major version of clang-format and clang-tidy (RULES.md D8); the
# exact release is pinned in tools/requirements-lint.txt.
set(TURBOREL_CPP_LLVM_TOOLS_MAJOR 23)

# Finds `tool` of the pinned major version into `out`, failing if it is
# missing or of another version.
function(turborel_cpp_find_llvm_tool out tool)
  find_program(${out} NAMES ${tool}-${TURBOREL_CPP_LLVM_TOOLS_MAJOR} ${tool} REQUIRED)
  execute_process(
    COMMAND "${${out}}" --version
    OUTPUT_VARIABLE version_text
    RESULT_VARIABLE result)
  if(NOT result EQUAL 0 OR NOT version_text MATCHES "version ${TURBOREL_CPP_LLVM_TOOLS_MAJOR}\\.")
    message(
      FATAL_ERROR
        "[DESIGN] RULES.md D8: ${${out}} is not ${tool} ${TURBOREL_CPP_LLVM_TOOLS_MAJOR}; "
        "install tools/requirements-lint.txt, or set ${out} to the pinned one")
  endif()
endfunction()

if(TURBOREL_CPP_LINT)
  turborel_cpp_find_llvm_tool(TURBOREL_CPP_CLANG_TIDY clang-tidy)
  turborel_cpp_find_llvm_tool(TURBOREL_CPP_CLANG_FORMAT clang-format)
  set(TURBOREL_CPP_TIDY_COMMAND "${TURBOREL_CPP_CLANG_TIDY}")
  if(APPLE AND NOT CMAKE_OSX_SYSROOT)
    # A clang-tidy that is not Apple's does not find the SDK Apple's
    # compiler finds by itself, so it is told where the SDK is.
    execute_process(
      COMMAND xcrun --show-sdk-path
      OUTPUT_VARIABLE sdk
      OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY)
    list(APPEND TURBOREL_CPP_TIDY_COMMAND "--extra-arg=-isysroot${sdk}")
  endif()
endif()

# Runs clang-tidy on each source of `target` as it builds, when linting.
function(turborel_cpp_tidy target)
  if(TURBOREL_CPP_LINT)
    set_target_properties(${target} PROPERTIES CXX_CLANG_TIDY "${TURBOREL_CPP_TIDY_COMMAND}")
  endif()
endfunction()

# Adds the checks that run as tests: the source rules always, with the
# tests; the format and the docs when linting.
function(turborel_cpp_checks)
  if(NOT TURBOREL_CPP_BUILD_TESTS)
    return()
  endif()
  add_test(NAME check.sources COMMAND "${CMAKE_COMMAND}" "-DROOT=${PROJECT_SOURCE_DIR}" -P
                                      "${PROJECT_SOURCE_DIR}/cmake/CheckSources.cmake")
  if(NOT TURBOREL_CPP_LINT)
    return()
  endif()
  set(format_args "-DROOT=${PROJECT_SOURCE_DIR}" "-DCLANG_FORMAT=${TURBOREL_CPP_CLANG_FORMAT}")
  add_test(NAME check.format COMMAND "${CMAKE_COMMAND}" ${format_args} -P
                                     "${PROJECT_SOURCE_DIR}/cmake/CheckFormat.cmake")
  add_custom_target(
    format
    COMMAND "${CMAKE_COMMAND}" ${format_args} -DFIX=ON -P "${PROJECT_SOURCE_DIR}/cmake/CheckFormat.cmake"
    COMMENT "Formatting the sources with clang-format"
    VERBATIM)
  turborel_cpp_docs()
endfunction()

# Adds the docs target, and the test that every public item is documented
# (R-DOC-1), where Doxygen is found.
function(turborel_cpp_docs)
  if(TURBOREL_CPP_REQUIRE_DOXYGEN)
    find_package(Doxygen REQUIRED)
  else()
    find_package(Doxygen)
  endif()
  if(NOT DOXYGEN_FOUND)
    message(STATUS "Doxygen not found: R-DOC-1 is checked where it is, as in CI")
    return()
  endif()
  set(DOXYGEN_PROJECT_NAME "turborel-cpp")
  set(DOXYGEN_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/doc")
  set(DOXYGEN_WARN_IF_UNDOCUMENTED YES)
  set(DOXYGEN_WARN_IF_DOC_ERROR YES)
  set(DOXYGEN_WARN_NO_PARAMDOC YES)
  set(DOXYGEN_WARN_AS_ERROR FAIL_ON_WARNINGS)
  set(DOXYGEN_EXTRACT_ALL NO)
  set(DOXYGEN_EXTRACT_PRIVATE NO)
  set(DOXYGEN_GENERATE_LATEX NO)
  set(DOXYGEN_ENABLE_PREPROCESSING YES)
  set(DOXYGEN_MACRO_EXPANSION YES)
  set(DOXYGEN_EXPAND_ONLY_PREDEF YES)
  set(DOXYGEN_PREDEFINED "TURBOREL_CPP_EXPORT=")
  set(DOXYGEN_EXCLUDE_SYMBOLS "detail")
  doxygen_add_docs(docs "${PROJECT_SOURCE_DIR}/include" COMMENT "Building the API reference")
  add_test(NAME check.docs COMMAND "${DOXYGEN_EXECUTABLE}" "${PROJECT_BINARY_DIR}/Doxyfile.docs"
           WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
endfunction()
