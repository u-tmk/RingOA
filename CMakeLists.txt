cmake_minimum_required(VERSION 3.18)
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

project(RingOA VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)


# =========================
# Options
# =========================
option(RINGOA_BUILD_TESTS  "Build RingOA tests"  ON)
option(RINGOA_BUILD_BENCH  "Build RingOA bench"  ON)

# User-overridable knobs (empty means: let RingOA decide per-config)
set(LOG_LEVEL "" CACHE STRING "Override RingOA log level (0..5). Empty = per-config default.")
set(USE_FIXED_RANDOM_SEED "" CACHE STRING "Override fixed seed (0 or 1). Empty = per-config default.")


# =========================
# Output / Compiler Settings
# =========================
set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-ignored-attributes")


# =========================
# Dependencies
# =========================
set(THIRD_PARTY_DIR ${CMAKE_SOURCE_DIR}/thirdparty)
find_package(cryptoTools CONFIG REQUIRED HINTS ${THIRD_PARTY_DIR}/unix)


# =========================
# Build Type & Flags
# =========================
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

if(NOT CMAKE_BUILD_TYPE MATCHES "Debug|Release|RelWithDebInfo")
  message(FATAL_ERROR "Invalid build type: ${CMAKE_BUILD_TYPE}. Use Debug, Release, or RelWithDebInfo.")
endif()


# =========================
# Export Settings
# =========================
set(RINGOA_EXPORT_SET RingOATargets)
set(RINGOA_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/RingOA")


# =========================
# Info
# =========================
message(STATUS "--------------------------------------------")
message(STATUS " Project Configuration Info:")
message(STATUS "--------------------------------------------")
message(STATUS "[Project Information]")
message(STATUS "  Project Name              : ${PROJECT_NAME}")
message(STATUS "  Version                   : ${PROJECT_VERSION}")
message(STATUS "  CMake Version             : ${CMAKE_VERSION}")
message(STATUS "[Build Settings]")
message(STATUS "  Build Type                : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Install Prefix            : ${CMAKE_INSTALL_PREFIX}")
message(STATUS "[Directories]")
message(STATUS "  Source Directory          : ${CMAKE_SOURCE_DIR}")
message(STATUS "  Binary Directory          : ${CMAKE_CURRENT_BINARY_DIR}")
message(STATUS "[Libraries]")
message(STATUS "  cryptoTools Directory     : ${cryptoTools_DIR}")


# =========================
# Build Targets
# =========================
add_subdirectory(RingOA)

if(RINGOA_BUILD_TESTS)
  add_subdirectory(RingOA_Tests)
endif()

if(RINGOA_BUILD_BENCH)
  add_subdirectory(RingOA_Bench)
endif()

if(RINGOA_BUILD_TESTS OR RINGOA_BUILD_BENCH)
  add_subdirectory(frontend)
endif()


# =========================
# Install & Package Config
# =========================
install(
  EXPORT ${RINGOA_EXPORT_SET}
  NAMESPACE RingOA::
  DESTINATION "${RINGOA_INSTALL_CMAKEDIR}"
)

configure_package_config_file(
  "${CMAKE_CURRENT_LIST_DIR}/cmake/RingOAConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/RingOAConfig.cmake"
  INSTALL_DESTINATION "${RINGOA_INSTALL_CMAKEDIR}"
  NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/RingOAConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMajorVersion
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/RingOAConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/RingOAConfigVersion.cmake"
  DESTINATION "${RINGOA_INSTALL_CMAKEDIR}"
)
