#include <gtest/gtest.h>
#include "kctl/ring_buffer_lockfree.hpp"
#include <chrono>
#include <thread>
#include <vector>
#include <algorithm>
#include <iostream>

// As README notes: on a single-core sandbox the same test run twice
// produced p99 latencies differing by 50x -- that variance is scheduler
// noise, not signal, so this test does not assert bounds.
TEST(LatencyBenchmark, SpscPushToPopLatencyPercentiles) {
    constexpr int kSamples = 100'000;
    kctl::RingBufferLockfree<std::chrono::steady_clock::time_point> rb(1024);

    std::vector<long long> latencies_ns;
    latencies_ns.reserve(kSamples);

    std::thread producer([&] {
        for (int i = 0; i < kSamples; ++i) {
            auto t = std::chrono::steady_clock::now();
            while (!rb.try_push(t)) std::this_thread::yield();
        }
    });

    std::thread consumer([&] {
        int received = 0;
        while (received < kSamples) {
            if (auto t = rb.try_pop()) {
                auto now = std::chrono::steady_clock::now();
                latencies_ns.push_back(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(now - *t).count());
                ++received;
            } else {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    std::sort(latencies_ns.begin(), latencies_ns.end());
    auto pct = [&](double p) {
        size_t idx = static_cast<size_t>(p * (latencies_ns.size() - 1));
        return latencies_ns[idx];
    };

    std::cout << "[LatencyBenchmark] " << kSamples << " samples\n";
    std::cout << "  p50:  " << pct(0.50) << " ns\n";
    std::cout << "  p99:  " << pct(0.99) << " ns\n";
    std::cout << "  p999: " << pct(0.999) << " ns\n";

    SUCCEED();
}
