#include <gtest/gtest.h>
#include "kctl/device_handle.hpp"
#include "kctl/testing/mock_device_backend.hpp"

using kctl::DeviceHandle;
using kctl::testing::MockDeviceBackend;

TEST(DeviceHandle, ConstructsWithValidFd) {
    MockDeviceBackend backend;
    DeviceHandle h(backend, "/dev/fake0", 0);
    EXPECT_GE(h.fd(), 0);
    EXPECT_EQ(backend.open_calls(), 1);
    EXPECT_TRUE(backend.is_open(h.fd()));
}

TEST(DeviceHandle, DestructorClosesFd) {
    MockDeviceBackend backend;
    int captured_fd;
    {
        DeviceHandle h(backend, "/dev/fake0", 0);
        captured_fd = h.fd();
    }
    EXPECT_FALSE(backend.is_open(captured_fd));
    EXPECT_EQ(backend.close_calls(), 1);
}

TEST(DeviceHandle, MoveConstructTransfersOwnershipNoDoubleClose) {
    MockDeviceBackend backend;
    DeviceHandle a(backend, "/dev/fake0", 0);
    int fd = a.fd();

    DeviceHandle b(std::move(a));
    EXPECT_EQ(b.fd(), fd);
    EXPECT_EQ(backend.close_calls(), 0);
    EXPECT_TRUE(backend.is_open(fd));
}

TEST(DeviceHandle, OpenFailureThrowsAndNeverCallsClose) {
    MockDeviceBackend backend;
    backend.set_fail_open(true);
    EXPECT_THROW(DeviceHandle(backend, "/dev/nope", 0), std::system_error);
    EXPECT_EQ(backend.close_calls(), 0);
}

TEST(DeviceHandle, ScopeEndAfterMoveDoesNotDoubleClose) {
    MockDeviceBackend backend;
    int fd;
    {
        DeviceHandle a(backend, "/dev/fake0", 0);
        fd = a.fd();
        DeviceHandle b(std::move(a));
        (void)b;
    }
    EXPECT_EQ(backend.close_calls(), 1);
    EXPECT_FALSE(backend.is_open(fd));
}
