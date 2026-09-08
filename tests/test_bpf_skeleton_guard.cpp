#include <gtest/gtest.h>
#include "kctl/bpf_skeleton_guard.hpp"
#include "kctl/testing/mock_bpf_backend.hpp"

using kctl::BpfSkeletonGuard;
using kctl::testing::MockBpfBackend;
using kctl::testing::FakeSkel;

TEST(BpfSkeletonGuard, SuccessPathAttachesThenDetachesAndDestroysOnce) {
    MockBpfBackend backend(MockBpfBackend::FailAt::None);
    {
        BpfSkeletonGuard<FakeSkel> guard(backend);
        EXPECT_NE(guard.get(), nullptr);
    }
    EXPECT_EQ(backend.open_calls, 1);
    EXPECT_EQ(backend.load_calls, 1);
    EXPECT_EQ(backend.attach_calls, 1);
    EXPECT_EQ(backend.detach_calls, 1);
    EXPECT_EQ(backend.destroy_calls, 1);
}

TEST(BpfSkeletonGuard, OpenFailureThrowsAndCallsNothingElse) {
    MockBpfBackend backend(MockBpfBackend::FailAt::Open);
    EXPECT_THROW(BpfSkeletonGuard<FakeSkel> guard(backend), std::runtime_error);
    EXPECT_EQ(backend.load_calls, 0);
    EXPECT_EQ(backend.destroy_calls, 0);
}

TEST(BpfSkeletonGuard, LoadFailureThrowsDestroysButNeverDetaches) {
    MockBpfBackend backend(MockBpfBackend::FailAt::Load);
    EXPECT_THROW(BpfSkeletonGuard<FakeSkel> guard(backend), std::runtime_error);
    EXPECT_EQ(backend.attach_calls, 0);
    EXPECT_EQ(backend.detach_calls, 0);
    EXPECT_EQ(backend.destroy_calls, 1);
}

TEST(BpfSkeletonGuard, AttachFailureThrowsDestroysButNeverDetaches) {
    MockBpfBackend backend(MockBpfBackend::FailAt::Attach);
    EXPECT_THROW(BpfSkeletonGuard<FakeSkel> guard(backend), std::runtime_error);
    EXPECT_EQ(backend.detach_calls, 0);
    EXPECT_EQ(backend.destroy_calls, 1);
}
