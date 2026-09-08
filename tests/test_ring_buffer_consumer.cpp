#include <gtest/gtest.h>
#include "kctl/ring_buffer_consumer.hpp"
#include "kctl/testing/mock_ring_buffer_poll_source.hpp"
#include <chrono>
#include <thread>
#include <vector>

using kctl::RingBufferConsumer;
using kctl::testing::MockRingBufferPollSource;

TEST(RingBufferConsumer, ConsumesAllEventsInOrderWhenQueueNeverFull) {
    constexpr int kTotal = 500;
    MockRingBufferPollSource source(kTotal, 8);
    RingBufferConsumer<int> consumer(source, 1024);

    consumer.start();

    std::vector<int> consumed;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (static_cast<int>(consumed.size()) < kTotal &&
           std::chrono::steady_clock::now() < deadline) {
        if (auto v = consumer.try_consume()) {
            consumed.push_back(*v);
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
    consumer.stop();

    ASSERT_EQ(consumed.size(), static_cast<size_t>(kTotal));
    for (int i = 0; i < kTotal; ++i) {
        EXPECT_EQ(consumed[i], i) << "order violated at index " << i;
    }
    EXPECT_EQ(consumer.dropped_count(), 0u);
}

TEST(RingBufferConsumer, DropsOnFullQueueRatherThanBlocking) {
    constexpr int kTotal = 2000;
    MockRingBufferPollSource source(kTotal, 64);
    RingBufferConsumer<int> consumer(source, 16);

    consumer.start();

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (!source.drained() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    ASSERT_TRUE(source.drained());
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    consumer.stop();

    EXPECT_GT(consumer.dropped_count(), 0u);
    EXPECT_LT(consumer.dropped_count(), static_cast<size_t>(kTotal));
}
