include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(kctl_tests
  test_device_handle.cpp
  test_bpf_skeleton_guard.cpp
  test_ring_buffer_locked.cpp
  test_ring_buffer_lockfree.cpp
  test_mpmc_bounded_queue.cpp
  test_ring_buffer_consumer.cpp
  test_thread_pool_mpmc.cpp
  test_thread_pool_sharded.cpp
  test_device_task.cpp
  test_ring_buffer_compare.cpp
  test_thread_pool_compare.cpp
  test_latency_benchmark.cpp
)
target_link_libraries(kctl_tests PRIVATE kctl gtest_main pthread)

include(GoogleTest)
gtest_discover_tests(kctl_tests DISCOVERY_MODE PRE_TEST)
