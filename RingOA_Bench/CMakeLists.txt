# =========================
# Build RingOA Bench
# =========================

option(RINGOA_INSTALL_BENCH "Install RingOA bench library and headers" ON)

# Create the unified static library
add_library(RingOA_Bench STATIC)
add_library(RingOA::Bench ALIAS RingOA_Bench)

# Ensure consumers get C++20
target_compile_features(RingOA_Bench PUBLIC cxx_std_20)

set(RINGOA_BENCH_SOURCES
  seq_io.cpp
  gen_dataset.cpp
  ringoa_bench.cpp
  shared_ot_bench.cpp
  obliv_select_bench.cpp
  obliv_fmi_bench.cpp
  sot_fmi_bench.cpp
  obliv_range_bench.cpp
  sot_range_bench.cpp
)

target_sources(RingOA_Bench PRIVATE ${RINGOA_BENCH_SOURCES})

target_include_directories(RingOA_Bench
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/RingOA/RingOA_Bench>
)

target_compile_options(RingOA_Bench PRIVATE
  $<$<CONFIG:Debug>:-Og -g -Wall -Wextra>
  $<$<CONFIG:RelWithDebInfo>:-O2 -g -fno-omit-frame-pointer -march=native>
  $<$<CONFIG:Release>:-O2 -march=native -flto -funroll-loops>
)

target_link_libraries(RingOA_Bench
  PUBLIC
    RingOA::RingOA
    oc::tests_cryptoTools
)

set_target_properties(RingOA_Bench PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  VERSION   ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
  EXPORT_NAME Bench
)

if(RINGOA_INSTALL_BENCH)
  install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/RingOA/RingOA_Bench
    FILES_MATCHING
      PATTERN "*.h"  PATTERN "*.hpp" PATTERN "*.hh" PATTERN "*.ipp"
      PATTERN ".git" EXCLUDE
      PATTERN "CMakeFiles" EXCLUDE
  )

  install(TARGETS RingOA_Bench
    EXPORT ${RINGOA_EXPORT_SET}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/RingOA/RingOA_Bench
  )
endif()
