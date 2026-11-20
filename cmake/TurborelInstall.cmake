# Install and export rules (RULES.md D9): the library, its headers, the
# CMake package that gives `turborel::turborel-cpp` to `find_package`, and
# the license, which every package carries.
include_guard(GLOBAL)

include(CMakePackageConfigHelpers)

set(TURBOREL_CPP_PACKAGE_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/turborel-cpp")

install(
  TARGETS turborel-cpp
  EXPORT turborel-cpp-targets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  INCLUDES
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/turborel" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(FILES "${PROJECT_BINARY_DIR}/include/turborel/export.hpp" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/turborel")
install(
  EXPORT turborel-cpp-targets
  NAMESPACE turborel::
  FILE turborel-cpp-targets.cmake
  DESTINATION "${TURBOREL_CPP_PACKAGE_DIR}")

configure_package_config_file("${PROJECT_SOURCE_DIR}/cmake/turborel-cpp-config.cmake.in"
                              "${PROJECT_BINARY_DIR}/turborel-cpp-config.cmake" INSTALL_DESTINATION "${TURBOREL_CPP_PACKAGE_DIR}")
# A driver's version is the turborel release it is made for, and during
# the alpha nothing holds between releases, so only the same one matches
# (VERSIONING.md).
write_basic_package_version_file("${PROJECT_BINARY_DIR}/turborel-cpp-config-version.cmake" COMPATIBILITY ExactVersion)
install(FILES "${PROJECT_BINARY_DIR}/turborel-cpp-config.cmake" "${PROJECT_BINARY_DIR}/turborel-cpp-config-version.cmake"
        DESTINATION "${TURBOREL_CPP_PACKAGE_DIR}")
install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" "${PROJECT_SOURCE_DIR}/TRADEMARK.md"
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/doc/turborel-cpp")
