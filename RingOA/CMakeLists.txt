# =========================
# Build RingOA
# =========================

# Create the unified static library
add_library(RingOA STATIC)
add_library(RingOA::RingOA ALIAS RingOA)

# Ensure consumers get C++20
target_compile_features(RingOA PUBLIC cxx_std_20)

# All sources go directly into RingOA
target_sources(RingOA PRIVATE
  # utils
  utils/logger.cpp
  utils/timer.cpp
  utils/network.cpp

  # sharing
  sharing/beaver_triples_gen.cpp
  sharing/beaver_triples_io.cpp
  sharing/sharing_2p_ring.cpp
  sharing/sharing_2p_binary.cpp
  sharing/rep3_sharing_ring.cpp
  sharing/rep3_sharing_binary.cpp
  sharing/rep3_preprocess.cpp

  # fss
  fss/dcf_eval.cpp
  fss/dcf_gen.cpp
  fss/dcf_key.cpp
  fss/dpf_eval.cpp
  fss/dpf_gen.cpp
  fss/dpf_key.cpp
  fss/fss.cpp
  fss/prg.cpp

  # protocol
  protocol/context_2p.cpp
  protocol/context_3p.cpp
  protocol/ddcf.cpp
  protocol/equality.cpp
  protocol/integer_comparison.cpp
  protocol/min3.cpp
  protocol/obliv_select.cpp
  protocol/ringoa.cpp
  protocol/ringoa_fsc.cpp
  protocol/shared_ot.cpp
  protocol/zero_test.cpp

  # wm
  wm/plain_wm.cpp

  # obliv_fmi
  obliv_fmi/obliv_rank.cpp
  obliv_fmi/obliv_fmi.cpp
  obliv_fmi/sot_rank.cpp
  obliv_fmi/sot_fmi.cpp

  # obliv_range
  obliv_range/obliv_range.cpp
  obliv_range/sot_range.cpp
)

# Include headers from the RingOA/ tree
target_include_directories(RingOA
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)


# =========================
# Per-config compile options
# =========================
# Debug:       -Og -g -Wall -Wextra
# Release:     -O2 -march=native -flto -funroll-loops
# RelWithDebInfo: -O2 -g -fno-omit-frame-pointer -march=native
target_compile_options(RingOA PRIVATE
  $<$<CONFIG:Debug>:-Og -g -Wall -Wextra>
  $<$<CONFIG:Release>:-O2 -march=native -flto -funroll-loops>
  $<$<CONFIG:RelWithDebInfo>:-O2 -g -fno-omit-frame-pointer -march=native>
)


# ===== LOG_LEVEL (user override > per-config defaults) =====
if(DEFINED LOG_LEVEL AND NOT "${LOG_LEVEL}" STREQUAL "")
  target_compile_definitions(RingOA PUBLIC LOG_LEVEL=${LOG_LEVEL})
  set(LOG_LEVEL_EFFECTIVE "${LOG_LEVEL}")
else()
  target_compile_definitions(RingOA PUBLIC
    $<$<CONFIG:Debug>:LOG_LEVEL=5>
    $<$<CONFIG:RelWithDebInfo>:LOG_LEVEL=4>
    $<$<CONFIG:Release>:LOG_LEVEL=4>
  )
  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(LOG_LEVEL_EFFECTIVE 5)
  elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(LOG_LEVEL_EFFECTIVE 4)
  elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(LOG_LEVEL_EFFECTIVE 4)
  else()
    set(LOG_LEVEL_EFFECTIVE "(unknown)")
  endif()
endif()
set(LOG_LEVEL_EFFECTIVE ${LOG_LEVEL_EFFECTIVE} CACHE INTERNAL "Effective log level used by RingOA")
set(RingOA_LOG_LEVEL "${LOG_LEVEL_EFFECTIVE}" CACHE INTERNAL "Effective log level (public name)")

# ===== USE_FIXED_RANDOM_SEED (user override > per-config defaults) =====
if(DEFINED USE_FIXED_RANDOM_SEED AND NOT "${USE_FIXED_RANDOM_SEED}" STREQUAL "")
  target_compile_definitions(RingOA PUBLIC USE_FIXED_RANDOM_SEED=${USE_FIXED_RANDOM_SEED})
  set(USE_FIXED_RANDOM_SEED_EFFECTIVE "${USE_FIXED_RANDOM_SEED}")
else()
  target_compile_definitions(RingOA PUBLIC
    $<$<CONFIG:Debug>:USE_FIXED_RANDOM_SEED=1>
    $<$<CONFIG:RelWithDebInfo>:USE_FIXED_RANDOM_SEED=1>
    $<$<CONFIG:Release>:USE_FIXED_RANDOM_SEED=0>
  )
  if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(USE_FIXED_RANDOM_SEED_EFFECTIVE 1)
  elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    set(USE_FIXED_RANDOM_SEED_EFFECTIVE 0)
  else()
    set(USE_FIXED_RANDOM_SEED_EFFECTIVE "(unknown)")
  endif()
endif()
set(USE_FIXED_RANDOM_SEED_EFFECTIVE ${USE_FIXED_RANDOM_SEED_EFFECTIVE} CACHE INTERNAL "Effective fixed seed used by RingOA")
set(RingOA_USE_FIXED_RANDOM_SEED "${USE_FIXED_RANDOM_SEED_EFFECTIVE}" CACHE INTERNAL "Effective fixed-seed flag (public name)")

message(STATUS "[RingOA Settings]")
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  message(STATUS "  Compile Options           : -Og -g -Wall -Wextra")
elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
  message(STATUS "  Compile Options           : -O2 -march=native -flto -funroll-loops")
elseif (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
  message(STATUS "  Compile Options           : -O2 -g -fno-omit-frame-pointer -march=native")
endif()
message(STATUS "  LOG_LEVEL                 : ${LOG_LEVEL_EFFECTIVE}")
message(STATUS "  USE_FIXED_RANDOM_SEED     : ${USE_FIXED_RANDOM_SEED_EFFECTIVE}")
message(STATUS "--------------------------------------------")


# =========================
# Link external deps only once here
# =========================
target_link_libraries(RingOA
  PUBLIC
    oc::cryptoTools
    sdsl
    divsufsort
    divsufsort64
)

set_target_properties(RingOA PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  VERSION   ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
  EXPORT_NAME RingOA
)

install(
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/RingOA
  FILES_MATCHING
    PATTERN "*.h"  PATTERN "*.hpp" PATTERN "*.hh" PATTERN "*.ipp"
    PATTERN ".git" EXCLUDE
    PATTERN "CMakeFiles" EXCLUDE
)

install(TARGETS RingOA
  EXPORT ${RINGOA_EXPORT_SET}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/RingOA
)
