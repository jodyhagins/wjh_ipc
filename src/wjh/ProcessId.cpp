// ======================================================================
// Copyright 2025 Jody Hagins
// Distributed under the MIT Software License
// See accompanying file LICENSE or copy at
// https://opensource.org/licenses/MIT
// ======================================================================
#include "ProcessId.hpp"

#include <sstream>

#include <time.h>

#if defined(__linux__)
    #include <fstream>
#elif defined(__APPLE__) && defined(__MACH__)
    #include <sys/proc_info.h>

    #include <libproc.h>
#else
    #error "Unrecognized operating system"
#endif

namespace wjh {

namespace {

#if defined(__linux__)
std::optional<::timeval>
start_time_of(pid_t pid)
{
    errno = 0;
    // Read the start_time (in jiffies) from /proc/[pid]/stat
    std::string stat_path = "/proc/" + std::to_string(pid) + "/stat";
    std::ifstream stat_file(stat_path);
    if (not stat_file.is_open()) {
        return std::nullopt; // Process may not exist
    }

    // Extract the 22nd field (start time in jiffies)
    uint64_t start_time_jiffies = 0;
    {
        std::string token;
        for (int i = 0; i < 22; ++i) {
            if (not (stat_file >> token)) {
                return std::nullopt; // Malformed stat file
            }
        }
        start_time_jiffies = std::stoull(token);
    }

    // Get system HZ dynamically
    unsigned long hz;
    if (auto r = sysconf(_SC_CLK_TCK); r == -1) {
        return std::nullopt; // Unable to retrieve HZ
    } else {
        hz = static_cast<unsigned long>(r);
    }

    // Convert jiffies to seconds + microseconds
    struct timeval tv;
    tv.tv_sec = static_cast<::time_t>(start_time_jiffies / hz);
    tv.tv_usec = static_cast<::suseconds_t>(
        (start_time_jiffies % hz) * (1'000'000 / hz));

    // Get system boot time (CLOCK_BOOTTIME)
    struct timespec boot_ts, real_ts;
    if (clock_gettime(CLOCK_REALTIME, &real_ts) == -1 ||
        clock_gettime(CLOCK_BOOTTIME, &boot_ts) == -1)
    {
        return std::nullopt;
    }

    // Compute absolute start time (Unix timestamp)
    tv.tv_sec += real_ts.tv_sec - boot_ts.tv_sec;
    return tv;
}
#elif defined(__APPLE__) && defined(__MACH__)
std::optional<::timeval>
start_time_of(pid_t pid)
{
    errno = 0;
    struct proc_bsdinfo bsdinfo;
    int ret = proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &bsdinfo, sizeof(bsdinfo));
    if (std::size_t(ret) != sizeof(bsdinfo)) {
        return std::nullopt;
    }
    return ::timeval{
        .tv_sec = static_cast<::time_t>(bsdinfo.pbi_start_tvsec),
        .tv_usec = static_cast<::suseconds_t>(bsdinfo.pbi_start_tvusec)};
}
#endif

inline constexpr std::uint32_t epoch = 1'704'067'200; // 2024-01-01 00:00:00 UTC

template <typename T>
inline constexpr auto shift = std::numeric_limits<T>::digits / 2;

template <typename ValueT>
ValueT
value_from(pid_t pid, std::optional<::timeval> tv)
{
    if (not tv) {
        auto err = errno;
        std::stringstream strm;
        strm << "can't get process start time: pid=" << pid;
        if (err) {
            strm << ": " << strerror(errno);
        }
        throw std::runtime_error(strm.str());
    }

    if constexpr (std::is_same_v<ValueT, std::uint64_t>) {
        return (ValueT(pid) << shift<ValueT>) |
            std::uint32_t(tv->tv_sec - epoch);
    } else if constexpr (std::is_same_v<ValueT, __uint128_t>) {
        static_assert(shift<ValueT> == 64);
        return (ValueT(pid) << shift<ValueT>) |
            (1'000'000ul * std::uint64_t(tv->tv_sec) +
             std::uint64_t(tv->tv_usec));
    } else {
        return "Unknown ticks type representation";
    }
}

} // anonymous namespace

std::optional<ProcessId>
ProcessId::
maybe(pid_t pid)
{
    if (auto start_time = start_time_of(pid)) {
        ProcessId result;
        result.value_ = value_from<Value>(pid, start_time);
        return result;
    }
    return std::nullopt;
}

ProcessId::
ProcessId(pid_t pid)
: value_(value_from<Value>(pid, start_time_of(pid)))
{ }

ProcessId::
ProcessId(pid_t pid, ::timeval const & start_time) noexcept
: value_(value_from<Value>(pid, start_time))
{ }

pid_t
ProcessId::
pid() const
{
    return static_cast<pid_t>(value_ >> shift<Value>);
}

::timeval
ProcessId::
start_time() const
{
    if constexpr (std::is_same_v<Value, std::uint64_t>) {
        auto const ticks = std::uint32_t(value_);
        return {
            .tv_sec = static_cast<::time_t>(epoch + ticks),
            .tv_usec = static_cast<::suseconds_t>(0)};
    } else {
        auto const ticks = std::uint64_t(value_);
        return {
            .tv_sec = static_cast<::time_t>(ticks / 1'000'000ull),
            .tv_usec = static_cast<::suseconds_t>(ticks % 1'000'000ull)};
    }
}

} // namespace wjh
