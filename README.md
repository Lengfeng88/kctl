# kctl

Lock-free systems runtime library for latency-sensitive C++ (drivers,
profilers, and similar predictable-latency workloads). RAII resource
wrappers, bounded lock-free queues, and thread pools — all header-only,
all built around bounded/fixed-capacity data structures so there's no
dynamic allocation on the hot path.

This is a personal systems-programming project built module by module,
each one covered by GoogleTest and, where concurrency is involved,
verified data-race-free under ThreadSanitizer.

## Build & test

```bash
cmake -B build
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Requires a C++20 compiler (uses `concept` in `device_task.hpp`) and
CMake ≥ 3.16. GoogleTest is pulled automatically via `FetchContent`.

### ThreadSanitizer

```bash
cmake -B build-tsan -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=thread -g -O1 -fno-pie" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread -no-pie"
cmake --build build-tsan -j$(nproc)
./build-tsan/tests/kctl_tests --gtest_filter="-*Compare*:*Benchmark*"
```

`-fno-pie -no-pie` works around a PIE/ASLR incompatibility with TSan's
shadow memory layout seen on some Linux setups (GCC 13 / kernel 6.x).
If `gtest_discover_tests` crashes during the build itself, the
`tests/CMakeLists.txt` here already uses `DISCOVERY_MODE PRE_TEST` to
avoid running the TSan binary at configure time.

## Modules

| Header | What it does |
|---|---|
| `device_handle.hpp` | RAII device fd wrapper over an injectable `IDeviceBackend` |
| `bpf_skeleton_guard.hpp` | RAII BPF skeleton lifecycle (open→load→attach), rolls back exactly what succeeded on failure |
| `ring_buffer_locked.hpp` | Mutex-protected ring buffer — correctness/perf baseline |
| `ring_buffer_lockfree.hpp` | SPSC lock-free ring buffer, acquire/release only |
| `mpmc_bounded_queue.hpp` | Bounded MPMC lock-free queue, Vyukov design |
| `ring_buffer_consumer.hpp` | Decouples a single-threaded poll loop (e.g. `ring_buffer__poll`) from downstream consumers, drop-on-full |
| `thread_pool_mpmc.hpp` | Worker pool sharing one MPMC queue; `submit()` is multi-producer safe |
| `thread_pool_sharded.hpp` | Worker pool with one private SPSC queue per worker; zero cross-worker contention, but `submit()` is single-threaded only |
| `device_task.hpp` | Heterogeneous task dispatch via `concept DeviceTaskLike` + a hand-written function-pointer vtable — no `virtual` anywhere |
| `testing/mock_*.hpp` | In-memory backends for every interface above, so the suite needs no root, real device, or real kernel BPF program |

## Test coverage

39 GoogleTest cases, including multi-producer/multi-consumer stress
tests exercising millions of operations per test. All concurrent
structures (`ring_buffer_lockfree`, `mpmc_bounded_queue`,
`thread_pool_mpmc`, `thread_pool_sharded`, `device_task`) have been run
under ThreadSanitizer with zero warnings.

Three benchmark tests (`test_ring_buffer_compare.cpp`,
`test_thread_pool_compare.cpp`, `test_latency_benchmark.cpp`) print
throughput/latency numbers rather than asserting on them — these
numbers are hardware- and scheduler-dependent and shouldn't be read as
guarantees.

## Design note: why bounded, not unbounded

Every concurrent structure here is fixed-capacity. That's a deliberate
scope boundary, not an oversight: unbounded structures (e.g. a
Michael-Scott queue) need safe memory reclamation (hazard pointers or
epoch-based reclamation) to avoid use-after-free when one thread frees
a node another thread might still be reading. That's a different, and
harder, problem than anything solved here, and it's out of scope for
this project for now.

## Known gaps

- `device_task.hpp`'s `TaskScheduler` has no real caller in this repo
  yet — it's independently tested, but nothing here actually submits
  tasks to it.
- Every backend interface (`IDeviceBackend`, `IBpfBackend`,
  `IRingBufferPollSource`) currently has a mock implementation only,
  used for tests. No real `PosixDeviceBackend` or libbpf-backed poll
  source is implemented here.

## License

MIT — see [LICENSE](LICENSE).
