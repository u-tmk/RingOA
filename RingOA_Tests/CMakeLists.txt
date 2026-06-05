# =========================
# Build RingOA Tests
# =========================

option(RINGOA_INSTALL_TESTS "Install RingOA tests library and headers" ON)

# Create the unified static library
add_library(RingOA_Tests STATIC)
add_library(RingOA::Tests ALIAS RingOA_Tests)


# Ensure consumers get C++20
target_compile_features(RingOA_Tests PUBLIC cxx_std_20)

# ALL sources go directly into RingOA_Tests
set(RINGOA_TEST_SOURCES
  utils/utils_test.cpp
  utils/timer_test.cpp
  utils/network_test.cpp
  utils/file_io_test.cpp
  fss/prg_test.cpp
  fss/dpf_test.cpp
  fss/dcf_test.cpp
  sharing/sharing_2p_ring_test.cpp
  sharing/sharing_2p_binary_test.cpp
  sharing/rep3_sharing_ring_test.cpp
  sharing/rep3_sharing_binary_test.cpp
  protocol/equality_test.cpp
  protocol/integer_comparison_test.cpp
  protocol/min3_test.cpp
  protocol/zt_test.cpp
  protocol/obliv_select_test.cpp
  protocol/ringoa_test.cpp
  protocol/shared_ot_test.cpp
  protocol/ddcf_test.cpp
  wm/wm_test.cpp
  obliv_fmi/obliv_rank_test.cpp
  obliv_fmi/obliv_fmi_test.cpp
  obliv_fmi/sot_rank_test.cpp
  obliv_fmi/sot_fmi_test.cpp
  obliv_range/obliv_range_test.cpp
  obliv_range/sot_range_test.cpp
  unit_tests.cpp
)

target_sources(RingOA_Tests PRIVATE ${RINGOA_TEST_SOURCES})

target_include_directories(RingOA_Tests
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/RingOA/RingOA_Tests>
)

target_compile_options(RingOA_Tests PRIVATE
  $<$<CONFIG:Debug>:-Og -g -Wall -Wextra>
  $<$<CONFIG:RelWithDebInfo>:-O2 -g -fno-omit-frame-pointer -march=native>
  $<$<CONFIG:Release>:-O2 -march=native -flto -funroll-loops>
)

target_link_libraries(RingOA_Tests
  PUBLIC
    RingOA::RingOA
    oc::tests_cryptoTools
)

set_target_properties(RingOA_Tests PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  VERSION   ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
  EXPORT_NAME Tests
)

if(RINGOA_INSTALL_TESTS)
  install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/RingOA/RingOA_Tests
    FILES_MATCHING
      PATTERN "*.h"  PATTERN "*.hpp" PATTERN "*.hh" PATTERN "*.ipp"
      PATTERN ".git" EXCLUDE
      PATTERN "CMakeFiles" EXCLUDE
  )

  install(TARGETS RingOA_Tests
    EXPORT ${RINGOA_EXPORT_SET}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/RingOA/RingOA_Tests
  )
endif()
