// ======================================================================
// Copyright 2025 Jody Hagins
// Distributed under the MIT Software License
// See accompanying file LICENSE or copy at
// https://opensource.org/licenses/MIT
// ======================================================================
#define DOCTEST_CONFIG_ASSERTS_RETURN_VALUES
#include "wjh/ProcessIdLock.hpp"

#include "doctest.hpp"
#include "rapidcheck.hpp"

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <latch>
#include <new>
#include <random>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <unistd.h>

namespace {

TEST_SUITE("ProcessIdLock")
{
    using wjh::ProcessIdLock;

    // Put in the mmap file
    struct SharedData
    {
        // The lock being tested
        ProcessIdLock lock;
        // refcount to track uses of the file
        wjh::Atomic<std::size_t> refcount;

        // Misc - use in tests however
        int counter;
        ProcessIdLock another_lock;
    };

    static auto create_shared_lock_file = [] {
        auto [fd, path] = [] {
            auto p = std::filesystem::temp_directory_path() / "proc_ut_XXXXXX";
            auto s = p.string();
            auto bytes = std::vector<char>(s.begin(), s.end());
            bytes.push_back('\0');
            auto desc = ::mkstemp(bytes.data());
            REQUIRE(desc >= 0);
            if (ftruncate(desc, sizeof(SharedData)) != 0) {
                close(desc);
                FAIL("ftruncate");
            }
            return std::make_pair(desc, std::filesystem::path(bytes.data()));
        }();

        // Map file into memory
        void * addr = mmap(
            nullptr,
            sizeof(SharedData),
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            fd,
            0);
        close(fd);
        REQUIRE(addr != MAP_FAILED);

        auto p = ::new (addr) SharedData{};
        munmap(p, sizeof(SharedData));

        struct CleanupGuard
        {
            explicit CleanupGuard(std::filesystem::path p)
            : path(std::move(p))
            { }
            ~CleanupGuard()
            {
                if (not path.empty()) {
                    remove(path);
                }
            }
            void operator = (CleanupGuard &&) = delete;
            std::filesystem::path path;
        };
        return CleanupGuard(std::move(path));
    };

    static auto map_shared_lock_file = [](std::filesystem::path const & path) {
        int fd = open(path.c_str(), O_RDWR);
        REQUIRE(fd != -1);
        void * addr = mmap(
            nullptr,
            sizeof(SharedData),
            PROT_READ | PROT_WRITE,
            MAP_SHARED,
            fd,
            0);
        close(fd);
        REQUIRE(addr != MAP_FAILED);
        return reinterpret_cast<SharedData *>(addr);
    };

    static auto mapped = [](std::filesystem::path p) {
        struct Internal
        {
            explicit Internal(std::filesystem::path p)
            : shared(map_shared_lock_file(p))
            , path(std::move(p))
            {
                ++shared->refcount;
            }

            ~Internal()
            {
                if (--shared->refcount == 0u) {
                    remove(path);
                }
                munmap(shared, sizeof(SharedData));
            }
            void operator = (Internal &&) = delete;

            SharedData * operator -> () { return shared; }
            explicit operator bool () const { return shared; }

        private:
            SharedData * shared;
            std::filesystem::path path;
        };
        return Internal(std::move(p));
    };

    TEST_CASE("Can zero initialize")
    {
#if defined(__clang__)
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Walloca"
#endif
        auto const sz = sizeof(ProcessIdLock);
        auto const storage = alloca(sz);
        std::memset(storage, 0xff, sz);

        auto const check = alloca(sz);
        std::memset(check, 0xff, sz);
        auto lock = ::new (storage) ProcessIdLock;
        CHECK(std::memcmp(check, lock, sz) == 0);

        std::memset(check, 0x00, sz);
        lock = ::new (storage) ProcessIdLock{};
        CHECK(std::memcmp(check, lock, sz) == 0);
#if defined(__clang__)
    #pragma clang diagnostic pop
#endif
    }

    TEST_CASE("Can lock and unlock")
    {
        auto lock = ProcessIdLock{};
        REQUIRE(lock.try_lock());
        lock.unlock();
    }

    TEST_CASE("ProcessIdLock in file")
    {
        auto guard = create_shared_lock_file();
        auto shared = mapped(guard.path);
        REQUIRE(shared);

        SUBCASE("Basic Lock/Unlock")
        {
            REQUIRE(shared->lock.try_lock());
            shared->lock.unlock();
        }

        SUBCASE("Try-Lock Behavior")
        {
            REQUIRE(shared->lock.try_lock());
            CHECK(not shared->lock.try_lock()); // Second try should fail
            shared->lock.unlock();
            REQUIRE(shared->lock.try_lock()); // Now it should succeed again
            shared->lock.unlock();
        }

        SUBCASE("Exclusive Lock Enforcement")
        {
            auto t1_locked = std::latch{1};
            auto t1_done = std::latch{1};
            auto t2_ready = std::latch{1};
            std::thread t1([&] {
                shared->lock.lock();
                t1_locked.count_down();

                t2_ready.wait();

                shared->lock.unlock();
                t1_done.count_down();
            });

            std::thread t2([&] {
                t1_locked.wait();

                CHECK(not shared->lock.try_lock());
                t2_ready.count_down();

                t1_done.wait();

                REQUIRE(shared->lock.try_lock());
                shared->lock.unlock();
            });

            t1.join();
            t2.join();
        }

        SUBCASE("Process Crash Recovery")
        {
            // Parent gets lock.
            shared->lock.lock();

            pid_t pid = fork();
            if (pid == 0) {
                // Child waits for parent to release lock
                shared->lock.lock();
                // Child exits without releasing lock
                _exit(1);
            } else {
                // Parent releases lock and waits for child to exit
                shared->lock.unlock();

                // Here, the child is still running or is a zombie.
                auto expired = std::chrono::high_resolution_clock::now() +
                    std::chrono::seconds(30);
                while (std::chrono::high_resolution_clock::now() < expired) {
                    if (shared->lock.try_lock()) {
                        shared->lock.unlock();
                        break;
                    }
                }
                REQUIRE(shared->lock.try_lock());
                shared->lock.unlock();
                waitpid(pid, nullptr, 0);

                REQUIRE(shared->lock.try_lock());
                shared->lock.unlock();
            }
        }

        SUBCASE("Concurrent Locking with Threads")
        {
            int const num_threads = 10;
            auto latch = std::latch{num_threads};
            std::vector<std::thread> threads;
            for (int i = 0; i < num_threads; ++i) {
                threads.emplace_back([&] {
                    // Wait for all threads to be running
                    latch.count_down();
                    latch.wait();

                    // Now do work
                    for (int k = 0; k < 10000; ++k) {
                        std::this_thread::yield();
                        auto lock = std::lock_guard(shared->lock);
                        REQUIRE(shared->another_lock.try_lock());
                        shared->another_lock.unlock();
                        shared->counter++;
                    }
                });
            }
            for (auto & t : threads) {
                t.join();
            }

            REQUIRE(shared->lock.try_lock());
            CHECK(shared->counter == 10000 * num_threads);
            shared->lock.unlock();
        }

        SUBCASE("Concurrent Locking with Processes")
        {
            int const num_processes = 10;
            for (int i = 0; i < num_processes; ++i) {
                pid_t pid = fork();
                if (pid == 0) {
                    // Wait for all processes to be running
                    shared->lock.lock();
                    shared->counter++;
                    shared->lock.unlock();
                    for (;;) {
                        auto lock = std::lock_guard(shared->lock);
                        if (shared->counter >= num_processes) {
                            break;
                        }
                    }

                    // Now go
                    for (int k = 0; k < 10000; ++k) {
                        std::this_thread::yield();
                        auto lock = std::lock_guard(shared->lock);
                        REQUIRE(shared->another_lock.try_lock());
                        shared->another_lock.unlock();
                        shared->counter++;
                    }
                    _exit(0);
                }
            }

            for (int i = 0; i < num_processes; ++i) {
                wait(nullptr);
            }

            REQUIRE(shared->lock.try_lock());
            CHECK(shared->counter == num_processes * 10000 + num_processes);
            shared->lock.unlock();
        }
    }
}


} // anonymous namespace
