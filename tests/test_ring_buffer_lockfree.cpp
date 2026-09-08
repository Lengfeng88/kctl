#include <gtest/gtest.h>
#include "kctl/ring_buffer_lockfree.hpp"
#include <thread>
#include <vector>

using kctl::RingBufferLockfree;

TEST(RingBufferLockfree, EmptyPopReturnsNullopt) {
    RingBufferLockfree<int> rb(4);
    EXPECT_EQ(rb.try_pop(), std::nullopt);
}

TEST(RingBufferLockfree, PushPopPreservesOrder) {
    RingBufferLockfree<int> rb(4);
    EXPECT_TRUE(rb.try_push(10));
    EXPECT_TRUE(rb.try_push(20));
    EXPECT_TRUE(rb.try_push(30));
    EXPECT_EQ(rb.try_pop(), 10);
    EXPECT_EQ(rb.try_pop(), 20);
    EXPECT_EQ(rb.try_pop(), 30);
    EXPECT_EQ(rb.try_pop(), std::nullopt);
}

TEST(RingBufferLockfree, FullPushFails) {
    RingBufferLockfree<int> rb(2);
    EXPECT_TRUE(rb.try_push(1));
    EXPECT_TRUE(rb.try_push(2));
    EXPECT_FALSE(rb.try_push(3));
}

TEST(RingBufferLockfree, WraparoundAcrossCapacityBoundary) {
    RingBufferLockfree<int> rb(4);
    for (int round = 0; round < 3; ++round) {
        EXPECT_TRUE(rb.try_push(round * 10 + 1));
        EXPECT_TRUE(rb.try_push(round * 10 + 2));
        EXPECT_EQ(rb.try_pop(), round * 10 + 1);
        EXPECT_EQ(rb.try_pop(), round * 10 + 2);
    }
    EXPECT_EQ(rb.try_pop(), std::nullopt);
}

TEST(RingBufferLockfree, SpscStressNoLossNoDuplicationOrderPreserved) {
    constexpr size_t kCapacity = 1024;
    constexpr int kCount = 2'000'000;

    RingBufferLockfree<int> rb(kCapacity);
    std::vector<int> consumed;
    consumed.reserve(kCount);

    std::thread producer([&] {
        for (int i = 0; i < kCount; ++i) {
            while (!rb.try_push(i)) std::this_thread::yield();
        }
    });

    std::thread consumer([&] {
        int received = 0;
        while (received < kCount) {
            if (auto v = rb.try_pop()) {
                consumed.push_back(*v);
                ++received;
            } else {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    ASSERT_EQ(consumed.size(), static_cast<size_t>(kCount));
    for (int i = 0; i < kCount; ++i) {
        ASSERT_EQ(consumed[i], i) << "order violated at index " << i;
    }
}
