// ======================================================================
// Copyright 2025 Jody Hagins
// Distributed under the MIT Software License
// See accompanying file LICENSE or copy at
// https://opensource.org/licenses/MIT
// ======================================================================
#define DOCTEST_CONFIG_ASSERTS_RETURN_VALUES
#include "wjh/ProcessId.hpp"

#include <sys/wait.h>

#include <cstdlib>
#include <cstring>
#include <new>

#include <unistd.h>

#include "testing/doctest.hpp"
#include "testing/rapidcheck.hpp"

namespace {
using wjh::ProcessId;

TEST_SUITE("ProcessId")
{
    TEST_CASE("Can zero initialize")
    {
#if defined(__clang__)
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Walloca"
#endif
        auto const storage = alloca(sizeof(ProcessId));
        std::memset(storage, 0xff, sizeof(ProcessId));

        auto const check = alloca(sizeof(ProcessId));
        std::memset(check, 0xff, sizeof(ProcessId));
        auto id = ::new (storage) ProcessId;
        CHECK(std::memcmp(check, id, sizeof(ProcessId)) == 0);

        std::memset(check, 0x00, sizeof(ProcessId));
        id = ::new (storage) ProcessId{};
        CHECK(std::memcmp(check, id, sizeof(ProcessId)) == 0);
#if defined(__clang__)
    #pragma clang diagnostic pop
#endif
    }

    TEST_CASE("Can get my own id")
    {
        auto me = ProcessId(::getpid());
        CHECK(me.pid() == ::getpid());
        auto opt = ProcessId::maybe(::getpid());
        CHECK(opt) && CHECK(*opt == me);
    }

    TEST_CASE("Can get the other id")
    {
        auto the_parent = ProcessId(::getpid());
        if (auto pid = ::fork(); pid == 0) {
            auto parent_id = ProcessId(::getppid());
            auto opt_parent = ProcessId::maybe(::getppid());
            auto child_id = ProcessId(::getpid());
            auto opt_child = ProcessId::maybe(::getpid());
            int result = CHECK(the_parent == parent_id) &&
                    CHECK(the_parent != child_id) &&
                    CHECK(parent_id.pid() == ::getppid()) &&
                    CHECK(child_id.pid() == ::getpid()) && CHECK(opt_parent) &&
                    CHECK(opt_parent->pid() == ::getppid()) &&
                    CHECK(opt_child) && CHECK(opt_child->pid() == ::getpid())
                ? 42
                : -1;
            _Exit(result);
        } else {
            REQUIRE(pid != -1);
            auto parent_id = ProcessId(::getpid());
            CHECK(parent_id.pid() == ::getpid());
            auto opt_parent = ProcessId::maybe(::getpid());
            CHECK(opt_parent) && CHECK(opt_parent->pid() == ::getpid());
            auto child_id = ProcessId(pid);
            CHECK(child_id.pid() == pid);
            auto opt_child = ProcessId::maybe(pid);
            CHECK(opt_child) && CHECK(opt_child->pid() == pid);

            CHECK(the_parent == parent_id);
            CHECK(the_parent != child_id);

            int status = -1;
            REQUIRE(::waitpid(pid, &status, 0) == pid);
            CHECK(WIFEXITED(status)) && CHECK(WEXITSTATUS(status) == 42);
        }
    }

    TEST_CASE("No pid for dead process")
    {
        if (auto pid = ::fork(); pid == 0) {
            _Exit(0);
        } else {
            REQUIRE(pid != -1);
            int status = -1;
            REQUIRE(::waitpid(pid, &status, 0) == pid);

            auto parent_id = ProcessId(::getpid());
            CHECK(parent_id.pid() == ::getpid());
            CHECK(not ProcessId::maybe(pid));
            REQUIRE_THROWS(ProcessId(pid));
        }
    }

    auto to_string = [](::timeval const & tv) {
        char buffer[64];
        ::tm tm_time;
        localtime_r(&tv.tv_sec, &tm_time);
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm_time);
        auto result = std::string(buffer);
        snprintf(buffer, sizeof(buffer), ".%06d", int(tv.tv_usec));
        result += buffer;
        return result;
    };

    auto ignore = [](auto &&) {};

    TEST_CASE("Can get process start time")
    {
        std::array<int, 2> to_child, to_parent;
        REQUIRE(::pipe(to_child.data()) == 0);
        REQUIRE(::pipe(to_parent.data()) == 0);

        char ch_buf;
        ProcessId before_id;
        if (auto pid = ::fork(); pid == 0) {
            ignore(::write(to_parent[1], "x", 1));
            ignore(::read(to_child[0], &ch_buf, 1));
            _Exit(0);
        } else {
            REQUIRE(pid != -1);
            ignore(::read(to_parent[0], &ch_buf, 1));
            before_id = ProcessId(pid);
            ignore(::write(to_child[1], "x", 1));
            REQUIRE(::waitpid(pid, nullptr, 0) == pid);
        }

        ::timeval before, child_start{};
        gettimeofday(&before, nullptr);
        auto const expire = std::chrono::steady_clock::now() +
            std::chrono::seconds(30);
        while (std::chrono::steady_clock::now() < expire &&
               to_string(before) > to_string(child_start))
        {
            ::usleep(1000000 / 100);
            ::close(to_child[0]);
            ::close(to_child[1]);
            ::close(to_parent[0]);
            ::close(to_parent[1]);
            REQUIRE(::pipe(to_child.data()) == 0);
            REQUIRE(::pipe(to_parent.data()) == 0);
            if (auto pid = ::fork(); pid == 0) {
                ignore(::write(to_parent[1], "x", 1));
                ignore(::read(to_child[0], &ch_buf, 1));
                _Exit(0);
            } else {
                REQUIRE(pid != -1);
                ignore(::read(to_parent[0], &ch_buf, 1));
                auto child_id = ProcessId(pid);
                ignore(::write(to_child[1], "x", 1));
                REQUIRE(::waitpid(pid, nullptr, 0) == pid);
                REQUIRE(child_id.pid() == pid);
                child_start = child_id.start_time();
                CHECK(
                    to_string(before_id.start_time()) <=
                    to_string(child_start));
                if (to_string(before) <= to_string(child_start)) {
                    break;
                }
            }
        }
        CHECK(to_string(before) <= to_string(child_start));
    }

    TEST_CASE("Can construct with pid and start time")
    {
        std::array<int, 2> fd;
        REQUIRE(::pipe(fd.data()) == 0);
        if (auto pid = ::fork(); pid == 0) {
            char buf;
            ignore(::read(fd[0], &buf, 1));
            _Exit(0);
        } else {
            REQUIRE(pid != -1);
            auto child_id = ProcessId(pid);
            ignore(::write(fd[1], "x", 1));
            ::close(fd[0]);
            ::close(fd[1]);
            int status = -1;
            REQUIRE(::waitpid(pid, &status, 0) == pid);
            auto other = ProcessId(child_id.pid(), child_id.start_time());
            CHECK(other == child_id);
        }
    }
}

} // anonymous namespace
