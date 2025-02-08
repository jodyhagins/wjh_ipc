// ======================================================================
// Copyright 2025 Jody Hagins
// Distributed under the MIT Software License
// See accompanying file LICENSE or copy at
// https://opensource.org/licenses/MIT
// ======================================================================
#include "ProcessId.hpp"

#include <cstdio>
#include <cstring>
#include <limits>
#include <sstream>
#include <utility>

#include <pthread.h>
#include <time.h>

#if defined(__linux__)
    #include <sys/stat.h>
    #include <sys/types.h>

    #include <alloca.h>
    #include <fcntl.h>
    #include <unistd.h>
#elif defined(__APPLE__) && defined(__MACH__)
    #include <sys/proc_info.h>

    #include <libproc.h>
#else
    #error "Unrecognized operating system"
#endif

namespace wjh {

namespace {

#if defined(__linux__)
template <typename FnT, typename ErrT>
auto
with_file_contents(char const * fname, FnT && fn, ErrT && err)
{
    // Needs to be free of async-signal unsafe operations.
    int fd = ::open(fname, O_RDONLY);
    if (fd == -1) {
        return std::forward<ErrT>(err)();
    }

    #if defined(__clang__)
        #pragma clang diagnostic push
        #pragma clang diagnostic ignored "-Walloca"
    #endif
    // We can only read into the stack, but we don't know how much we need, so
    // we start with 8k and increase by 1k each time we don't get the entire
    // file of bytes.
    std::size_t size = 8 * 1024;
    for (;;) {
        auto buffer = static_cast<char *>(alloca(size));
        std::size_t n;
        if (auto nread = ::read(fd, buffer, size - 1); nread <= 0) {
            ::close(fd);
            return std::forward<ErrT>(err)();
        } else if ((n = static_cast<std::size_t>(nread)); n == size - 1) {
            size += 1024;
            ::lseek(fd, 0, SEEK_SET);
            continue;
        }
        buffer[n] = '\0';
        ::close(fd);

        if constexpr (std::is_invocable_v<FnT, char const *>) {
            return std::forward<FnT>(fn)(static_cast<char const *>(buffer));
        } else if constexpr (std::is_invocable_v<FnT, char *>) {
            return std::forward<FnT>(fn)(static_cast<char *>(buffer));
        } else if constexpr (std::is_invocable_v<FnT, std::string_view>) {
            return std::forward<FnT>(fn)(std::string_view(buffer, n));
        }
    }
    #if defined(__clang__)
        #pragma clang diagnostic pop
    #endif
}

::time_t
get_boot_time()
{
    // Needs to be free of async-signal unsafe operations.
    auto error = [] {
        // Um... Yeah.  Not async-signal, but at this point we don't care.
        char buf[256];
        ::snprintf(buf, sizeof(buf), "/proc/stat: %s", ::strerror(errno));
        throw std::runtime_error(buf);
    };
    return with_file_contents(
        "/proc/stat",
        [&](char const * buffer) {
            auto q = ::strstr(buffer, "\nbtime ");
            if (q) {
                q += 7;
            } else if (::strncmp(buffer, "btime ", 6) == 0) {
                q = buffer;
            } else {
                errno = ENOENT;
                error();
            }
            return ::atoi(q);
        },
        [&] { return error(), 0; });
}

static ::time_t const boot_time = get_boot_time();

std::optional<::timeval>
start_time_of(pid_t pid)
{
    // Needs to be free of async-signal unsafe operations.
    errno = 0;
    char fname[256];
    ::snprintf(fname, sizeof(fname), "/proc/%u/stat", unsigned(pid));

    return with_file_contents(
        fname,
        [](char const * buffer) -> std::optional<::timeval> {
            // Field 3 is the process state, 22 is the process start time
            char state = '\0';
            unsigned long long start_time = 0;
            if (sscanf(
                    buffer,
                    "%*d (%*[^)]) %c %*d %*d %*d %*d %*d %*u %*u %*u "
                    "%*u %*u %*u %*u %*d %*d %*d %*d %*d %*d %llu",
                    &state,
                    &start_time) != 2)
            {
                return std::nullopt;
            }

            switch (state) {
            case 'Z': // Zombie
            case 'X': // Dead (from Linux 2.6.0 onward)
            case 'x': // Dead (Linux 2.6.33 to 3.13 only)
                return std::nullopt;
            default:
                break;
            }

            // Get system HZ dynamically
            unsigned long hz;
            if (auto r = ::sysconf(_SC_CLK_TCK); r == -1) {
                return std::nullopt;
            } else {
                hz = static_cast<unsigned long>(r);
            }

            // Convert starttime ticks to seconds + microseconds
            ::timeval tv;
            tv.tv_sec = static_cast<::time_t>(start_time / hz);
            tv.tv_usec = static_cast<::suseconds_t>(
                (start_time % hz) * (1'000'000 / hz));

            tv.tv_sec += boot_time;
            return tv;
        },
        []() -> std::optional<::timeval> { return std::nullopt; });
}
#elif defined(__APPLE__) && defined(__MACH__)
std::optional<::timeval>
start_time_of(pid_t pid)
{
    // proc_pidinfo is a wrapper around a system call, does not allocate or do
    // anything that would make it async-signal unsafe.
    errno = 0;
    ::proc_bsdinfo bsdinfo;
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

ProcessId
ProcessId::
current()
{
    static ProcessId id = [] {
        ::pthread_atfork(nullptr, nullptr, [] { id = ProcessId{::getpid()}; });
        return ProcessId{::getpid()};
    }();
    return id;
}

} // namespace wjh
