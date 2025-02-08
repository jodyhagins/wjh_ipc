// ======================================================================
// Copyright 2025 Jody Hagins
// Distributed under the MIT Software License
// See accompanying file LICENSE or copy at
// https://opensource.org/licenses/MIT
// ======================================================================
#ifndef WJH_0f67a257f8f74a6583a279c42fe6225c
#define WJH_0f67a257f8f74a6583a279c42fe6225c

#include "Atomic.hpp"
#include "ProcessId.hpp"

#include <type_traits>

namespace wjh {

/**
 * An inter-process lock on a ProcessId object.
 *
 * This lock is based on atomic operations on a ProcessId, and function
 * similarly to a pthread robust lock.  However, it is implemented using
 * atomics, and should have similar semantics on any system that supports an
 * atomic ProcessId.  Furthermore, it is an implicit lifetime type, and can be
 * safely used in such contexts, including shared memory and mmap files.
 *
 * The main idea is that the owner of the lock is the process whose ProcessId
 * matches the value in the atomic variable.  This allows us to implement a form
 * of robust lock recovery in the case the owner of the lock dies while holding
 * the lock.  This also means the lock is process-level.
 *
 * @note  The lock will be forcable taken from a process that has been
 * determined to be dead.  A process could be seen as dead if it can't be seen
 * due to permissions, so make sure that all cooperating processes that use the
 * same lock can see each other.
 */
struct ProcessIdLock
{
    /**
     * Try to obtain the lock.
     *
     * @return  true if the calling thread obtained the lock; false otherwise.
     *
     * @pre  Calling thread does not already hold the lock.
     */
    bool try_lock();

    /**
     * Obtain the lock, blocking (busy-wait) until the lock has been obtained.
     *
     * @pre  Calling thread does not already hold the lock.
     */
    void lock();

    /**
     * Unlocks the lock.
     *
     * @pre  Calling thread holds the lock.
     */
    void unlock();

private:
    bool exchange(ProcessId & expected, ProcessId const & desired);
    bool try_lock_impl(ProcessId const & me);

    Atomic<ProcessId> pid_;
    static_assert(Atomic<ProcessId>::is_always_lock_free);
};

static_assert(std::is_trivially_constructible_v<ProcessIdLock>);

} // namespace wjh

#endif // WJH_0f67a257f8f74a6583a279c42fe6225c
