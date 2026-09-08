#include <gtest/gtest.h>
#include "kctl/ring_buffer_locked.hpp"
#include <thread>
#include <vector>
#include <atomic>

using kctl::RingBufferLocked;

TEST(RingBufferLocked, EmptyPopReturnsNullopt) {
    RingBufferLocked<int> rb(4);
    EXPECT_EQ(rb.try_pop(), std::nullopt);
}

TEST(RingBufferLocked, PushPopPreservesOrder) {
    RingBufferLocked<int> rb(4);
    EXPECT_TRUE(rb.try_push(1));
    EXPECT_TRUE(rb.try_push(2));
    EXPECT_TRUE(rb.try_push(3));
    EXPECT_EQ(rb.try_pop(), 1);
    EXPECT_EQ(rb.try_pop(), 2);
    EXPECT_EQ(rb.try_pop(), 3);
    EXPECT_EQ(rb.try_pop(), std::nullopt);
}

TEST(RingBufferLocked, FullPushFails) {
    RingBufferLocked<int> rb(2);
    EXPECT_TRUE(rb.try_push(1));
    EXPECT_TRUE(rb.try_push(2));
    EXPECT_FALSE(rb.try_push(3));
    EXPECT_EQ(rb.size(), 2u);
}

TEST(RingBufferLocked, WraparoundAcrossCapacityBoundary) {
    RingBufferLocked<int> rb(3);
    EXPECT_TRUE(rb.try_push(1));
    EXPECT_TRUE(rb.try_push(2));
    EXPECT_EQ(rb.try_pop(), 1);
    EXPECT_TRUE(rb.try_push(3));
    EXPECT_TRUE(rb.try_push(4)); // tail_ wraps here
    EXPECT_EQ(rb.try_pop(), 2);
    EXPECT_TRUE(rb.try_push(5));
    EXPECT_EQ(rb.try_pop(), 3);
    EXPECT_EQ(rb.try_pop(), 4);
    EXPECT_EQ(rb.try_pop(), 5);
    EXPECT_EQ(rb.try_pop(), std::nullopt);
}

TEST(RingBufferLocked, MultiProducerMultiConsumerNoLossNoDuplication) {
    constexpr int kProducers = 4;
    constexpr int kConsumers = 4;
    constexpr int kPerProducer = 20000;
    constexpr int kTotal = kProducers * kPerProducer;

    RingBufferLocked<int> rb(256);
    std::atomic<int> consumed_count{0};
    std::vector<std::atomic<int>> seen(kTotal);
    for (auto& s : seen) s = 0;

    std::vector<std::thread> producers, consumers;
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&, p] {
            for (int i = 0; i < kPerProducer; ++i) {
                int val = p * kPerProducer + i;
                while (!rb.try_push(val)) std::this_thread::yield();
            }
        });
    }
    for (int c = 0; c < kConsumers; ++c) {
        consumers.emplace_back([&] {
            while (consumed_count.load(std::memory_order_relaxed) < kTotal) {
                if (auto v = rb.try_pop()) {
                    seen[*v].fetch_add(1, std::memory_order_relaxed);
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
