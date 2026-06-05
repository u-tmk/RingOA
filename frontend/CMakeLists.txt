# =========================
# Build frontend runners
# =========================

add_executable(tests_RingOA test_main.cpp)
target_compile_options(tests_RingOA PRIVATE
  $<$<CONFIG:Debug>:-Og -g -Wall -Wextra>
  $<$<CONFIG:RelWithDebInfo>:-O2 -g -fno-omit-frame-pointer -march=native>
  $<$<CONFIG:Release>:-O2 -march=native -flto -funroll-loops>
)
target_link_libraries(tests_RingOA PRIVATE RingOA::RingOA RingOA::Tests)

add_executable(bench_RingOA bench_main.cpp)
target_compile_options(bench_RingOA PRIVATE
  $<$<CONFIG:Debug>:-Og -g -Wall -Wextra>
  $<$<CONFIG:RelWithDebInfo>:-O2 -g -fno-omit-frame-pointer -march=native>
  $<$<CONFIG:Release>:-O2 -march=native -flto -funroll-loops>
)
target_link_libraries(bench_RingOA PRIVATE RingOA::RingOA RingOA::Bench)
