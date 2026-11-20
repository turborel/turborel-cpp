# SPDX-License-Identifier: BSD-2-Clause-Patent
#
# turborel-cpp, the C++20 driver for turborel: a wrapper over turborel-c's
# C API, with no protocol code of its own (RULES.md D1). The version is the
# turborel release the driver is made for, the server's own (VERSIONING.md).
cmake_minimum_required(VERSION 3.25)

project(
  turborel-cpp
  VERSION 0.0.0
  DESCRIPTION "The C++ driver for turborel"
  HOMEPAGE_URL "https://turborel.com/docs/drivers/cpp/"
  LANGUAGES CXX)

include(CMakeDependentOption)
include(GNUInstallDirs)

option(TURBOREL_CPP_BUILD_TESTS "Build the tests (RULES.md D12)" ${PROJECT_IS_TOP_LEVEL})
option(TURBOREL_CPP_INSTALL "Generate the install and export rules (RULES.md D9)" ON)
option(TURBOREL_CPP_LINT "Run clang-tidy as the library builds, and the format, source and doc checks (RULES.md D13)" OFF)
option(TURBOREL_CPP_REQUIRE_DOXYGEN "Fail to configure without Doxygen when linting (RULES.md D13)" OFF)
option(TURBOREL_CPP_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer (RULES.md D14)" OFF)
option(TURBOREL_CPP_COVERAGE "Build with Clang's source-based coverage (RULES.md D7)" OFF)
# Off until turborel-c reaches its milestone M3 and has an API to wrap
# (RULES.md D11).
option(TURBOREL_CPP_WITH_C "Build against turborel-c (RULES.md D11)" OFF)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(TurborelWarnings)
include(TurborelTools)
include(TurborelDependencies)

add_library(turborel-cpp)
add_library(turborel::turborel-cpp ALIAS turborel-cpp)

target_sources(
  turborel-cpp
  PRIVATE src/data_error.cpp
          src/datetime.cpp
          src/decimal.cpp
          src/error.cpp
          src/int128.cpp
          src/interval.cpp
          src/uuid.cpp)
target_compile_features(turborel-cpp PUBLIC cxx_std_20)
target_include_directories(
  turborel-cpp PUBLIC "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
                      "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>"
                      "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
set_target_properties(
  turborel-cpp
  PROPERTIES EXPORT_NAME turborel-cpp
             CXX_EXTENSIONS OFF
             CXX_VISIBILITY_PRESET hidden
             VISIBILITY_INLINES_HIDDEN ON
             VERSION ${PROJECT_VERSION}
             SOVERSION ${PROJECT_VERSION_MAJOR})

include(GenerateExportHeader)
generate_export_header(
  turborel-cpp
  BASE_NAME
  turborel_cpp
  EXPORT_FILE_NAME
  "${PROJECT_BINARY_DIR}/include/turborel/export.hpp")
if(NOT BUILD_SHARED_LIBS)
  target_compile_definitions(turborel-cpp PUBLIC TURBOREL_CPP_STATIC_DEFINE)
endif()

turborel_cpp_warnings(turborel-cpp)
turborel_cpp_instrument(turborel-cpp)
turborel_cpp_tidy(turborel-cpp)
turborel_cpp_link_c(turborel-cpp)

if(TURBOREL_CPP_INSTALL)
  include(TurborelInstall)
endif()

if(TURBOREL_CPP_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()

turborel_cpp_checks()
