# The project's dependencies, each with its decision in RULES.md (R-DEP-1).
include_guard(GLOBAL)

include(FetchContent)

# turborel-c, the C driver this one wraps (RULES.md D1, D11): its CMake
# package `turborel`, target turborel::turborel. An installed package of
# the same release is found first, as `cmake --install`, vcpkg or Conan
# leave it; failing that, the pinned commit is fetched and built with the
# driver. The pin is no cache variable, so a build made before it moved
# takes the new one; a checkout of turborel-c's own is built instead by
# CMake's FETCHCONTENT_SOURCE_DIR_TURBOREL.
set(TURBOREL_CPP_C_REPOSITORY "https://github.com/turborel/turborel-c.git")
set(TURBOREL_CPP_C_COMMIT "90c56adcd5674e2cda4615ae1266ddcb7d2e5a7b")

# Links `target` to turborel-c.
function(turborel_cpp_link_c target)
  FetchContent_Declare(
    turborel
    GIT_REPOSITORY "${TURBOREL_CPP_C_REPOSITORY}"
    GIT_TAG "${TURBOREL_CPP_C_COMMIT}"
    SYSTEM FIND_PACKAGE_ARGS ${PROJECT_VERSION} EXACT CONFIG)
  # turborel-c builds a shared library unless told; fetched, it is built
  # static, into the driver, as this one is by default. Set here, in the
  # function's scope, so the driver's own BUILD_SHARED_LIBS is unchanged.
  set(BUILD_SHARED_LIBS OFF)
  # Its ODBC driver is a library of its own, which this driver does not
  # wrap.
  set(TRL_ODBC OFF)
  # Its build that lets a test fail any of its allocations, for the tests'
  # faults alone (RULES.md D14).
  set(TRL_FAULTS ${TURBOREL_CPP_FAULTS})
  FetchContent_MakeAvailable(turborel)
  foreach(c_target IN ITEMS turborel turborel_objects)
    if(TARGET ${c_target})
      # Not ours: built with its own warnings, which CMAKE_COMPILE_WARNING_AS_ERROR
      # of the driver's own builds does not reach.
      set_target_properties(${c_target} PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
    endif()
  endforeach()
  target_link_libraries(${target} PUBLIC turborel::turborel)
endfunction()

# Catch2, the tests' framework (RULES.md D12), at a pinned release whose
# archive is checked by its SHA-256.
set(TURBOREL_CPP_CATCH2_VERSION 3.16.1)
set(TURBOREL_CPP_CATCH2_SHA256 d16bd1b8b2364918bd472cb216394acb93bde6784890df0d91bccfb3f28d777d)

# Makes Catch2's targets and its `catch_discover_tests` available.
macro(turborel_cpp_fetch_catch2)
  # Catch2 is built in the tests' language level, or it leaves out what it
  # prints of C++17 and C++20 types, such as std::string_view.
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  FetchContent_Declare(
    Catch2
    URL "https://github.com/catchorg/Catch2/archive/refs/tags/v${TURBOREL_CPP_CATCH2_VERSION}.tar.gz"
    URL_HASH SHA256=${TURBOREL_CPP_CATCH2_SHA256}
    DOWNLOAD_EXTRACT_TIMESTAMP ON
    SYSTEM)
  FetchContent_MakeAvailable(Catch2)
  # Catch2 is not ours: built with its own flags, and its warnings never
  # fail our build.
  set_target_properties(Catch2 Catch2WithMain PROPERTIES COMPILE_WARNING_AS_ERROR OFF)
  list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
  include(Catch)
endmacro()
