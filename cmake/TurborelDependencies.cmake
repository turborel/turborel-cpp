# The project's dependencies, each with its decision in RULES.md (R-DEP-1).
include_guard(GLOBAL)

include(FetchContent)

# turborel-c, the C driver this one wraps (RULES.md D11). Its installed
# CMake package is found first, as vcpkg, Conan or `cmake --install` leave
# it; failing that, its pinned commit is fetched.
set(TURBOREL_CPP_C_REPOSITORY
    "https://github.com/turborel/turborel-c.git"
    CACHE STRING "Where turborel-c is fetched from (RULES.md D11)")
set(TURBOREL_CPP_C_COMMIT
    ""
    CACHE STRING "The full commit of turborel-c fetched (RULES.md D11)")

# Links `target` to turborel-c, when the driver is built against it.
function(turborel_cpp_link_c target)
  if(NOT TURBOREL_CPP_WITH_C)
    return()
  endif()
  if(TURBOREL_CPP_C_COMMIT STREQUAL "")
    find_package(turborel-c ${PROJECT_VERSION} EXACT CONFIG REQUIRED)
  else()
    FetchContent_Declare(
      turborel-c
      GIT_REPOSITORY "${TURBOREL_CPP_C_REPOSITORY}"
      GIT_TAG "${TURBOREL_CPP_C_COMMIT}"
      SYSTEM FIND_PACKAGE_ARGS ${PROJECT_VERSION} EXACT CONFIG)
    FetchContent_MakeAvailable(turborel-c)
  endif()
  target_link_libraries(${target} PUBLIC turborel::turborel-c)
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
