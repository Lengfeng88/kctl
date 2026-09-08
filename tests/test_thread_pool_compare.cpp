#include <gtest/gtest.h>
#include "kctl/thread_pool_mpmc.hpp"
#include "kctl/thread_pool_sharded.hpp"
#include <chrono>
#include <atomic>
#include <thread>
#include <iostream>

namespace {

long long benchmark_mpmc_submit_us(int num_workers, int ops) {
    kctl::ThreadPoolMpmc pool(num_workers, 4096);
    std::atomic<int> done{0};

    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < ops; ++i) {
        while (!pool.submit([&done] { done.fetch_add(1, std::memory_order_relaxed); })) {
            std::this_thread::yield();
        }
    }
    while (done.load(std::memory_order_relaxed) < ops) std::this_thread::yield();
    auto end = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
}

long long benchmark_sharded_submit_us(int num_workers, int ops) {
    kctl::ThreadPoolSharded pool(num_workers, 1024);
    std::atomic<int> done{0};

    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < ops; ++i) {
        while (!pool.submit([&done] { done.fetch_add(1, std::memory_order_relaxed); })) {
            std::this_thread::yield();
        }
    }
    while (done.load(std::memory_order_relaxed) < ops) std::this_thread::yield();
    auto end = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
}

} // namespace

TEST(ThreadPoolCompare, MpmcVsShardedSingleThreadedSubmitThroughput) {
    constexpr int kWorkers = 4;
    constexpr int kOps = 200'000;

    auto mpmc_us = benchmark_mpmc_submit_us(kWorkers, kOps);
    auto sharded_us = benchmark_sharded_submit_us(kWorkers, kOps);

    std::cout << "[ThreadPoolCompare] " << kOps << " tasks, " << kWorkers << " workers, single submitter\n";
    std::cout << "  mpmc (shared queue):  " << mpmc_us    << " us\n";
    std::cout << "  sharded (per-worker): " << sharded_us << " us\n";

    SUCCEED();
}
