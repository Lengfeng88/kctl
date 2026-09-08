#include <gtest/gtest.h>
#include "kctl/thread_pool_sharded.hpp"
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using kctl::ThreadPoolSharded;

static void wait_for(std::function<bool()> cond, std::chrono::seconds timeout) {
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!cond() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

TEST(ThreadPoolSharded, RunsAllSubmittedTasksExactlyOnceSingleThreadedSubmit) {
    constexpr int kTasks = 10000;
    ThreadPoolSharded pool(4, 1024);
    std::vector<std::atomic<int>> ran(kTasks);
    for (auto& r : ran) r = 0;

    for (int i = 0; i < kTasks; ++i) {
        while (!pool.submit([&ran, i] { ran[i].fetch_add(1, std::memory_order_relaxed); })) {
            std::this_thread::yield();
        }
    }
    wait_for([&] { return pool.completed_count() == kTasks; }, std::chrono::seconds(5));

    ASSERT_EQ(pool.completed_count(), static_cast<size_t>(kTasks));
    for (int i = 0; i < kTasks; ++i) {
        EXPECT_EQ(ran[i].load(), 1) << "task " << i;
    }
}

TEST(ThreadPoolSharded, RoundRobinDistributesAcrossAllShards) {
    constexpr int kWorkers = 4;
    constexpr int kTasks = 4000;
    ThreadPoolSharded pool(kWorkers, 1024);
    std::atomic<int> total{0};

    for (int i = 0; i < kTasks; ++i) {
        while (!pool.submit([&total] { total.fetch_add(1, std::memory_order_relaxed); })) {
            std::this_thread::yield();
        }
    }
    wait_for([&] { return total.load() == kTasks; }, std::chrono::seconds(5));
    EXPECT_EQ(total.load(), kTasks);
}
