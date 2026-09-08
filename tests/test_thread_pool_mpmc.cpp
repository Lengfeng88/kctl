#include <gtest/gtest.h>
#include "kctl/thread_pool_mpmc.hpp"
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using kctl::ThreadPoolMpmc;

static void wait_for(std::function<bool()> cond, std::chrono::seconds timeout) {
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!cond() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

TEST(ThreadPoolMpmc, RunsAllSubmittedTasksExactlyOnce) {
    constexpr int kTasks = 10000;
    ThreadPoolMpmc pool(4, 4096);
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

TEST(ThreadPoolMpmc, ConcurrentSubmittersFromMultipleThreadsSafely) {
    constexpr int kSubmitters = 4;
    constexpr int kPerSubmitter = 5000;
    constexpr int kTotal = kSubmitters * kPerSubmitter;

    ThreadPoolMpmc pool(4, 4096);
    std::atomic<int> total_ran{0};

    std::vector<std::thread> submitters;
    for (int s = 0; s < kSubmitters; ++s) {
        submitters.emplace_back([&] {
            for (int i = 0; i < kPerSubmitter; ++i) {
                while (!pool.submit([&total_ran] {
                    total_ran.fetch_add(1, std::memory_order_relaxed);
                })) {
                    std::this_thread::yield();
                }
            }
        });
    }
    for (auto& t : submitters) t.join();

    wait_for([&] { return total_ran.load() == kTotal; }, std::chrono::seconds(5));
    EXPECT_EQ(total_ran.load(), kTotal);
}
