// ======================================================================
// Copyright 2025 Jody Hagins
// Distributed under the MIT Software License
// See accompanying file LICENSE or copy at
// https://opensource.org/licenses/MIT
// ======================================================================
#ifndef WJH_aba1d3ca548948319db14098b22e039b
#define WJH_aba1d3ca548948319db14098b22e039b

#include <sys/time.h>

#include <atomic>
#include <cstdint>
#include <optional>
#include <type_traits>

#include <unistd.h>

namespace wjh {

/**
 * An expanded process-id.
 *
 * The traditional pid_t is relatively small, and can wrap.  For long-running
 * systems, we want to be able to identify processes with a bit more precision.
 *
 * This process id is composed of two pieces: the process id (pid_t) and the
 * start time of the process in "ticks."  While theoretically possible, it is
 * highly unlikely that two different processes share the same ProcessId.
 *
 * This type is an implicit lifetime type, and can be used as such, e.g., in
 * shared memory.
 *
 * One primary goal is that this identifier can be atomically updated, which
 * means that there is a restriction on size.  In cases where lock-free atomic
 * operations can be performed on 128-bit values, the timestamp will be as
 * detailed as the system allows.  Otherwise, the 64 bits must be used for both
 * the process id and the start time, which means that the start time resolution
 * is only kept in seconds.
 *
 * @note  This has been implemented and tested under linux and MacOS.  However,
 * there are some cases where discovering the start time of a process can't be
 * performed, even though the process is running.  In such a case, this class
 * acts as if there is no process with that ID.  This shouldn't be a problem in
 * normal cases where processes are part of a cooperative nexus, but it can
 * produce strange results if processes are running with different credentials
 * or elevated privileges.
 */
struct ProcessId
{
    /**
     * Trivial default constructor.
     */
    ProcessId() = default;

    /**
     * Construct the identifier for a running process.
     *
     * @param pid  The id for a currently running process.
     *
     * @throw  std::runtime_error if the calling process can't get start-time
     * information about the process for whatever reason (e.g., not running or
     * calling process does not have sufficient permissions). specific @p pid.
     */
    explicit ProcessId(pid_t pid);

    /**
     * Construct the identifier with a pid and start time.
     *
     * The process does not have to be running.  This constructor is used mainly
     * for serialization.
     *
     * @param pid  The native id for a process.
     *
     * @param start_time  The time the process started.
     *
     * @note  The constructed instance is only guaranteed to compare equal to
     * another ProcessId if the input to this function came from a ProcessId
     * that was constructed with just the pid of a running process.
     */
    explicit ProcessId(pid_t pid, ::timeval const & start_time) noexcept;

    /**
     * Get the system id of the process.
     *
     * @note  This call returns the pid_t that is encoded in the internal
     * representation of the process.  The process does not have to be running.
     */
    pid_t pid() const;

    /**
     * Get the start time of the process.
     *
     * @note  This call returns the time that is encoded in the internal
     * representation of the process.  The process does not have to be running.
     *
     * @note  The start time will have different granularity, depending on the
     * operating system and configuration.  For MacOS, it will have a
     * microsecond granularity.  For Linux, the clock HZ will be used to convert
     * from jiffies to time, so the granularity will be the best allowed by the
     * HZ settings.
     */
    ::timeval start_time() const;

    /**
     * Get a ProcessId for a specific running process.
     *
     * @return  A ProcessId, if one can be created.
     *
     * @note  A falsey return does not mean that a process with the requested @p
     * pid is not running. There could be a process with that @p pid active, but
     * the caller does not have permissions sufficient to query the expected
     * information.
     */
    static std::optional<ProcessId> maybe(pid_t pid);

    /**
     * A null/zero ProcessId.
     */
    static ProcessId null() { return ProcessId{}; }

    /**
     * The ProcessId for the calling process.
     */
    static ProcessId current();

    auto operator <=> (ProcessId const &) const = default;

private:
    using Value = std::conditional_t<
        std::atomic<__uint128_t>::is_always_lock_free,
        __uint128_t,
        std::uint64_t>;
    Value value_;
};

static_assert(std::is_trivially_default_constructible_v<ProcessId>);
static_assert(std::atomic<ProcessId>::is_always_lock_free);

} // namespace wjh

#endif // WJH_aba1d3ca548948319db14098b22e039b
