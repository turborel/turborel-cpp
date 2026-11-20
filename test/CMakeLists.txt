# The tests (RULES.md D12, R-TEST): one Catch2 binary of a source file per
# area, each test a CTest test of its own; and the install package, built
# and used as a program that finds it would.
turborel_cpp_fetch_catch2()

add_executable(turborel-cpp-tests error.cpp)
target_link_libraries(turborel-cpp-tests PRIVATE turborel::turborel-cpp Catch2::Catch2WithMain)
set_target_properties(turborel-cpp-tests PROPERTIES CXX_EXTENSIONS OFF)
turborel_cpp_warnings(turborel-cpp-tests)
turborel_cpp_tidy(turborel-cpp-tests)

set(test_properties "")
if(TURBOREL_CPP_COVERAGE)
  set(profiles "${PROJECT_BINARY_DIR}/profiles")
  set(test_properties PROPERTIES ENVIRONMENT "LLVM_PROFILE_FILE=${profiles}/%p-%m.profraw")
  # llvm-profdata and llvm-cov of the compiler's own LLVM, which may be
  # installed under versioned names, as on Ubuntu.
  string(REGEX MATCH "^[0-9]+" llvm_major "${CMAKE_CXX_COMPILER_VERSION}")
  add_custom_target(
    coverage-report
    COMMAND "${CMAKE_COMMAND}" "-DPROFILES=${profiles}"
            "-DBINARIES=$<TARGET_FILE:turborel-cpp-tests>$<$<BOOL:${BUILD_SHARED_LIBS}>:$<SEMICOLON>$<TARGET_FILE:turborel-cpp>>"
            "-DROOT=${PROJECT_SOURCE_DIR}" "-DLLVM_MAJOR=${llvm_major}" -P "${PROJECT_SOURCE_DIR}/cmake/Coverage.cmake"
    COMMENT "Reporting coverage against its floors"
    VERBATIM)
endif()

catch_discover_tests(turborel-cpp-tests DISCOVERY_MODE PRE_TEST ${test_properties})

# The package test needs a library its consumer can link as it is: not one
# built for sanitizers or coverage, whose runtimes the consumer lacks.
if(TURBOREL_CPP_INSTALL
   AND NOT TURBOREL_CPP_SANITIZE
   AND NOT TURBOREL_CPP_COVERAGE)
  set(stage "${PROJECT_BINARY_DIR}/stage")
  add_test(NAME package.install COMMAND "${CMAKE_COMMAND}" --install "${PROJECT_BINARY_DIR}" --prefix "${stage}"
                                        --config $<CONFIG>)
  add_test(
    NAME package.consume
    COMMAND
      "${CMAKE_CTEST_COMMAND}" --build-and-test "${CMAKE_CURRENT_SOURCE_DIR}/package"
      "${PROJECT_BINARY_DIR}/package" --build-generator "${CMAKE_GENERATOR}" --build-config $<CONFIG>
      --build-options "-DCMAKE_PREFIX_PATH=${stage}" "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
      "-DCMAKE_BUILD_TYPE=$<CONFIG>" --test-command consumer)
  set_tests_properties(package.install PROPERTIES FIXTURES_SETUP package)
  set_tests_properties(package.consume PROPERTIES FIXTURES_REQUIRED package)
endif()
