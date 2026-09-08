#include <gtest/gtest.h>
#include "kctl/ring_buffer_locked.hpp"
#include "kctl/ring_buffer_lockfree.hpp"
#include <chrono>
#include <thread>
#include <iostream>

namespace {

template <typename RingBuffer>
long long benchmark_spsc_throughput_us(RingBuffer& rb, int ops) {
    auto start = std::chrono::steady_clock::now();

    std::thread producer([&] {
        for (int i = 0; i < ops; ++i) {
            while (!rb.try_push(i)) std::this_thread::yield();
        }
    });
    std::thread consumer([&] {
        int received = 0;
        while (received < ops) {
            if (rb.try_pop()) ++received;
            else std::this_thread::yield();
        }
    });
    producer.join();
    consumer.join();

    auto end = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
}

} // namespace

// NOTE: prints numbers for a human to read on the target machine --
// deliberately does NOT assert lockfree < locked. Scheduler noise on a
// shared/virtualized/single-core box can dominate the difference.
TEST(RingBufferCompare, LockedVsLockfreeThroughput) {
    constexpr int kOps = 1'000'000;

    kctl::RingBufferLocked<int> locked(1024);
    kctl::RingBufferLockfree<int> lockfree(1024);

    auto locked_us = benchmark_spsc_throughput_us(locked, kOps);
    auto lockfree_us = benchmark_spsc_throughput_us(lockfree, kOps);

    std::cout << "[RingBufferCompare] " << kOps << " SPSC ops\n";
    std::cout << "  locked:   " << locked_us   << " us ("
              << (kOps * 1000.0 / locked_us)   << " Kops/s)\n";
    std::cout << "  lockfree: " << lockfree_us << " us ("
              << (kOps * 1000.0 / lockfree_us) << " Kops/s)\n";

    SUCCEED(); // presence of output above is the point of this test
}
