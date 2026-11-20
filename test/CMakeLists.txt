# The tests (RULES.md D12, R-TEST): one Catch2 binary of a source file per
# area, each test a CTest test of its own; and the install package, built
# and used as a program that finds it would.
turborel_cpp_fetch_catch2()

add_executable(
  turborel-cpp-tests
  support/certs.cpp
  support/frames.cpp
  support/server.cpp
  support/sockets.cpp
  values.cpp
  frames.cpp
  types.cpp
  connect.cpp
  session.cpp
  statements.cpp
  control.cpp
  lobs.cpp
  copy.cpp
  import.cpp
  settings.cpp
  pipeline.cpp
  pool.cpp
  agent.cpp
  faults.cpp
  standard.cpp
  datetime.cpp
  decimal.cpp
  error.cpp
  int128.cpp
  interval.cpp
  uuid.cpp)
# Named pipes are Windows's alone.
if(WIN32)
  target_sources(turborel-cpp-tests PRIVATE pipes.cpp)
endif()
target_link_libraries(turborel-cpp-tests PRIVATE turborel::turborel-cpp Catch2::Catch2WithMain)
target_compile_definitions(
  turborel-cpp-tests PRIVATE "TURBOREL_CPP_FIXTURES=\"${CMAKE_CURRENT_SOURCE_DIR}/fixtures\""
                             "TURBOREL_CPP_LIVE_DIR=\"${CMAKE_CURRENT_BINARY_DIR}/live\"")
# The live tests' certificates are made by OpenSSL, turborel-c's own TLS
# library, where the build found it; without it the TLS tests are skipped.
find_package(OpenSSL 3 COMPONENTS Crypto SSL)
if(OpenSSL_FOUND)
  target_compile_definitions(turborel-cpp-tests PRIVATE TURBOREL_CPP_TEST_CERTS)
  target_link_libraries(turborel-cpp-tests PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()
set_target_properties(turborel-cpp-tests PROPERTIES CXX_EXTENSIONS OFF)
turborel_cpp_warnings(turborel-cpp-tests)
turborel_cpp_tidy(turborel-cpp-tests)

set(test_properties "")
# turborel-c fetched and built to fail its allocations on purpose: the
# tests of faults run (RULES.md D14). Of an installed turborel-c, they are
# skipped.
if(TURBOREL_CPP_FAULTS AND TARGET turborel_objects)
  target_compile_definitions(turborel-cpp-tests PRIVATE TURBOREL_CPP_FAULTS)
endif()

# Each test's process starts the live tests' server on the driver's one
# port (R-TEST-8), so no two run at once, whatever CTest's -j asks.
set(test_properties PROPERTIES RESOURCE_LOCK turborel-cpp-live-server)
if(TURBOREL_CPP_COVERAGE)
  set(profiles "${PROJECT_BINARY_DIR}/profiles")
  list(APPEND test_properties ENVIRONMENT "LLVM_PROFILE_FILE=${profiles}/%p-%m.profraw")
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
