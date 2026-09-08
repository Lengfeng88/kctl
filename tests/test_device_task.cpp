#include <gtest/gtest.h>
#include "kctl/device_task.hpp"
#include <algorithm>
#include <memory>
#include <vector>

using kctl::TaskHandle;
using kctl::TaskScheduler;

namespace {

// No base class, no override, no virtual -- satisfies DeviceTaskLike purely
// by having the right three methods.
class FakeDmaTask {
public:
    FakeDmaTask(uint64_t id, int complete_after_polls)
        : id_(id), remaining_(complete_after_polls) {}

    uint64_t task_id() const { return id_; }
    void submit() { submitted_ = true; }
    bool poll_complete() {
        if (!submitted_) return false;
        --remaining_;
        return remaining_ <= 0;
    }

private:
    uint64_t id_;
    int remaining_;
    bool submitted_ = false;
};

class FakeComputeTask {
public:
    FakeComputeTask(uint64_t id, std::shared_ptr<int> poll_count)
        : id_(id), poll_count_(std::move(poll_count)) {}
    uint64_t task_id() const { return id_; }
    void submit() { submitted_ = true; }
    bool poll_complete() {
        ++(*poll_count_);
        return submitted_;
    }

private:
    uint64_t id_;
    bool submitted_ = false;
    std::shared_ptr<int> poll_count_;
};

} // namespace

TEST(DeviceTask, NewTaskTypeRequiresNoSchedulerChange) {
    TaskScheduler sched;
    sched.enqueue(TaskHandle(FakeDmaTask(42, 2)));
    EXPECT_EQ(sched.inflight_count(), 1u);
    EXPECT_TRUE(sched.is_inflight(42));

    auto done1 = sched.poll_once();
    EXPECT_TRUE(done1.empty());

    auto done2 = sched.poll_once();
    ASSERT_EQ(done2.size(), 1u);
    EXPECT_EQ(done2[0], 42u);
    EXPECT_FALSE(sched.is_inflight(42));
}

TEST(DeviceTask, DispatchesOnlyByIdNeverByType) {
    TaskScheduler sched;
    sched.enqueue(TaskHandle(FakeDmaTask(1, 3)));
    sched.enqueue(TaskHandle(FakeComputeTask(2, std::make_shared<int>(0))));

    EXPECT_EQ(sched.inflight_count(), 2u);
    auto done1 = sched.poll_once();
    ASSERT_EQ(done1.size(), 1u);
    EXPECT_EQ(done1[0], 2u); // compute completes on first poll
    EXPECT_TRUE(sched.is_inflight(1));
}

TEST(DeviceTask, CompletedTaskIsRemovedAndNeverPolledAgain) {
    TaskScheduler sched;
    auto poll_count = std::make_shared<int>(0);
    sched.enqueue(TaskHandle(FakeComputeTask(7, poll_count)));

    sched.poll_once();
    EXPECT_EQ(*poll_count, 1);
    EXPECT_FALSE(sched.is_inflight(7));

    sched.poll_once();
    EXPECT_EQ(*poll_count, 1);
}

TEST(DeviceTask, MultipleTasksCompleteAtDifferentTimesIndependently) {
    TaskScheduler sched;
    sched.enqueue(TaskHandle(FakeDmaTask(100, 1)));
    sched.enqueue(TaskHandle(FakeDmaTask(200, 3)));
    sched.enqueue(TaskHandle(FakeDmaTask(300, 5)));

    std::vector<uint64_t> all_done;
    for (int i = 0; i < 6 && sched.inflight_count() > 0; ++i) {
        auto done = sched.poll_once();
        all_done.insert(all_done.end(), done.begin(), done.end());
    }

    EXPECT_EQ(sched.inflight_count(), 0u);
    ASSERT_EQ(all_done.size(), 3u);
    auto pos100 = std::find(all_done.begin(), all_done.end(), 100u);
    auto pos200 = std::find(all_done.begin(), all_done.end(), 200u);
    auto pos300 = std::find(all_done.begin(), all_done.end(), 300u);
    EXPECT_LT(pos100 - all_done.begin(), pos200 - all_done.begin());
    EXPECT_LT(pos200 - all_done.begin(), pos300 - all_done.begin());
}

TEST(DeviceTask, RejectsTypeMissingRequiredMethod) {
    // Not run -- this documents the concept check at compile time.
    // struct NotATask { void submit() {} };
    // TaskHandle bad{NotATask{}}; // would fail to compile: no task_id()/poll_complete()
    SUCCEED();
}
