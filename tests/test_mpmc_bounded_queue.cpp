#include <gtest/gtest.h>
#include "kctl/mpmc_bounded_queue.hpp"
#include <thread>
#include <vector>
#include <atomic>

using kctl::MpmcBoundedQueue;

TEST(MpmcBoundedQueue, EmptyPopFails) {
    MpmcBoundedQueue<int> q(4);
    int out;
    EXPECT_FALSE(q.try_pop(out));
}

TEST(MpmcBoundedQueue, PushPopPreservesOrderSingleThreaded) {
    MpmcBoundedQueue<int> q(4);
    EXPECT_TRUE(q.try_push(1));
    EXPECT_TRUE(q.try_push(2));
    EXPECT_TRUE(q.try_push(3));
    int out;
    EXPECT_TRUE(q.try_pop(out)); EXPECT_EQ(out, 1);
    EXPECT_TRUE(q.try_pop(out)); EXPECT_EQ(out, 2);
    EXPECT_TRUE(q.try_pop(out)); EXPECT_EQ(out, 3);
    EXPECT_FALSE(q.try_pop(out));
}

TEST(MpmcBoundedQueue, FullPushFails) {
    MpmcBoundedQueue<int> q(2);
    EXPECT_TRUE(q.try_push(1));
    EXPECT_TRUE(q.try_push(2));
    EXPECT_FALSE(q.try_push(3));
}

TEST(MpmcBoundedQueue, WraparoundAcrossCapacityBoundary) {
    MpmcBoundedQueue<int> q(4);
    int out;
    for (int round = 0; round < 5; ++round) {
        EXPECT_TRUE(q.try_push(round));
        EXPECT_TRUE(q.try_pop(out));
        EXPECT_EQ(out, round);
    }
}

TEST(MpmcBoundedQueue, MultiProducerMultiConsumerNoLossNoDuplication) {
    constexpr int kProducers = 4;
    constexpr int kConsumers = 4;
    constexpr int kPerProducer = 250000;
    constexpr int kTotal = kProducers * kPerProducer;

    MpmcBoundedQueue<int> q(1024);
    std::vector<std::atomic<int>> seen(kTotal);
    for (auto& s : seen) s = 0;
    std::atomic<int> consumed_count{0};

    std::vector<std::thread> producers, consumers;
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&, p] {
            for (int i = 0; i < kPerProducer; ++i) {
                int val = p * kPerProducer + i;
                while (!q.try_push(val)) std::this_thread::yield();
            }
        });
    }
    for (int c = 0; c < kConsumers; ++c) {
        consumers.emplace_back([&] {
            int val;
            while (consumed_count.load(std::memory_order_relaxed) < kTotal) {
                if (q.try_pop(val)) {
                    seen[val].fetch_add(1, std::memory_order_relaxed);
                    consumed_count.fetch_add(1, std::memory_order_relaxed);
                } else {
                    std::this_thread::yield();
                }
            }
        });
    }
    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();

    for (int i = 0; i < kTotal; ++i) {
        EXPECT_EQ(seen[i].load(), 1) << "value " << i << " seen " << seen[i].load() << " times";
    }
}

TEST(MpmcBoundedQueue, CellAlignmentTemplateParameterCompiles) {
    MpmcBoundedQueue<int, 64> q(4);
    int out;
    EXPECT_TRUE(q.try_push(42));
    EXPECT_TRUE(q.try_pop(out));
    EXPECT_EQ(out, 42);
}
