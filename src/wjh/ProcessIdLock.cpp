// ======================================================================
// Copyright 2025 Jody Hagins
// Distributed under the MIT Software License
// See accompanying file LICENSE or copy at
// https://opensource.org/licenses/MIT
// ======================================================================
#include "ProcessIdLock.hpp"

#include <thread>

namespace wjh {

bool
ProcessIdLock::
try_lock()
{
    return try_lock_impl(PID::current());
}

void
ProcessIdLock::
lock()
{
    auto const me = PID::current();
    while (not try_lock_impl(me)) {
        std::this_thread::yield();
    }
}

void
ProcessIdLock::
unlock()
{
    // Even though we should own it, we should do an atomic exchange.
    auto me = PID::current();
    [[maybe_unused]] auto released = exchange(me, PID::null());

    // The assert just checks the pid ; not thread...
    assert(released);
}

bool
ProcessIdLock::
exchange(ProcessId & expected, ProcessId const & desired)
{
    return pid_.compare_exchange_strong(expected, desired);
}

bool
ProcessIdLock::ProcessIdLock::
try_lock_impl(ProcessId const & me)
{
    auto expected = PID::null();
    if (exchange(expected, me)) {
        // I got the lock
        return true;
    }

    if (expected != me) {
        // Check to see if the process is still alive.
        if (auto p = ProcessId::maybe(expected.pid()); not p || expected != *p)
        {
            // A process with that PID can't be found, or it has a different
            // value, which means it reclaimed the PID of a previous
            // process.  In reality, it could have been not found because of
            // permission issues, but we assume that processes cooperating
            // on the file and lock can see each other well enough. Either
            // way, we peel the lock from its cold dead hands.
            exchange(expected, PID::null());

            // And try to acquire the lock.
            expected = PID::null();
            if (exchange(expected, me)) {
                // I got the lock
                return true;
            }
        }
    }

    return false;
}

} // namespace wjh
