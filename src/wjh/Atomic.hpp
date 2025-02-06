// ======================================================================
// Copyright 2025 Jody Hagins
// Distributed under the MIT Software License
// See accompanying file LICENSE or copy at
// https://opensource.org/licenses/MIT
// ======================================================================
#ifndef WJH_8176d027a70b4e488f5aaf7a4492587e
#define WJH_8176d027a70b4e488f5aaf7a4492587e

#include "detail/Atomic.hpp"

#include <cstdint>

namespace wjh {

/**
 * A type designed for safe atomic operations across process boundaries through
 * various IPC mechanisms including shared memory and memory-mapped files.
 *
 * In C++20, std::atomic was modified to enforce zero-initialization through its
 * default constructor.  While this change improved safety for single-process
 * use cases, it inadvertently made std::atomic unsuitable for interprocess
 * communication scenarios by removing its trivial default constructor and
 * implicit lifetime characteristics.
 *
 * Thus, to use atomics in shared memory, without UB, one must resort to
 * stuffing the raw types into shared memory, then using the native functions or
 * std::atomic_ref.  This is not the worst thing in the world, but it would
 * surely be convenient to use an atomic encapsulation directly in shared
 * memory.
 *
 * Unlike std::atomic, Atomic maintains trivial default construction and
 * implicit lifetime characteristics necessary for cross-process usage.
 *
 * @tparam T  The type must statisfy all of these properties
 *     + Trivially default constructible
 *     + Implicit lifetime type
 *     + std::atomic<T>::is_always_lock_free
 *     + Trivially copyable
 *     + Copy constructible
 *     + Move constructible
 *     + Copy assignable
 *     + Move assignable
 *     + Neither const nor volatile qualified
 */
template <typename T>
requires std::is_trivially_default_constructible_v<T> &&
    atomic_detail::implicit_lifetime<T> &&
    std::atomic<T>::is_always_lock_free && std::is_trivially_copyable_v<T> &&
    std::is_copy_constructible_v<T> && std::is_move_constructible_v<T> &&
    std::is_copy_assignable_v<T> && std::is_move_assignable_v<T> &&
    std::is_same_v<T, std::remove_cv_t<T>>
class Atomic
: public atomic_detail::atomic_base<T>
{
    using base = atomic_detail::atomic_base<T>;
    T value_;

public:
    /**
     * The type atomic operations are applied.
     */
    using value_type = T;

    /**
     * Yield true if atomic operations are always lock free.
     *
     * @note  By definition, this will always be true.
     */
    static constexpr bool is_always_lock_free = true;

    /**
     * Trivially default construct an Atomic object.
     */
    constexpr Atomic() noexcept = default;

    /**
     * Construct with an explicit value for the underlying object.
     *
     * @param desired  The initial value of the underlying value.
     *
     * @note  The initialization is not atomic.
     *
     * @note  For initialization in shared memory, this is what will usually be
     * called, since the creation and setup of the shared memory isn't atomic
     * relative to this instance anyway.  The trivial default constructor is so
     * that it can be recognized as an implicit lifetime type.
     */
    constexpr Atomic(T desired) noexcept
    : value_{desired}
    { }

    /**
     * This type is neither copyable nor moveable.
     */
    constexpr void operator = (Atomic &&) = delete;

    /**
     * Assign @p desired to the atomic variable.
     *
     * Equivalent to store(desired).
     *
     * @note  Unlike most assignment operators, the assignment operators for
     * atomic types do not return a reference to their left-hand arguments. They
     * return a copy of the stored value instead.
     */
    constexpr T operator = (T desired) noexcept
    {
        store(desired);
        return desired;
    }

    /**
     * true if the atomic operations on the objects of this type are lock-free,
     * false otherwise.
     *
     *
     * @note  By definition, this will always return true.
     */
    constexpr bool is_lock_free() const noexcept { return true; }

    /**
     * Atomically replaces the current value with @p desired. Memory is affected
     * according to the value of @p order.
     *
     * @pre  order is one of std::memory_order_relaxed,
     * std::memory_order_release, or std::memory_order_seq_cst.
     */
    constexpr void store(
        T desired,
        std::memory_order order = std::memory_order_seq_cst) noexcept
    {
        if constexpr (std::is_fundamental_v<T>) {
            __atomic_store_n(
                std::addressof(value_),
                desired,
                atomic_detail::native_order<
                    __ATOMIC_RELAXED,
                    __ATOMIC_RELEASE,
                    __ATOMIC_SEQ_CST>(order));
        } else {
            __atomic_store(
                std::addressof(value_),
                std::addressof(desired),
                atomic_detail::native_order<
                    __ATOMIC_RELAXED,
                    __ATOMIC_ACQUIRE,
                    __ATOMIC_CONSUME,
                    __ATOMIC_SEQ_CST>(order));
        }
    }

    /**
     * Atomically loads and returns the current value of the atomic variable.
     *
     * Memory is affected according to the value of @p order.
     *
     * @pre  order is one of std::memory_order_relaxed,
     * std::memory_order_acquire, std::memory_order_consume, or
     * std::memory_order_seq_cst.
     */
    constexpr T load(
        std::memory_order order = std::memory_order_seq_cst) const noexcept
    {
        if constexpr (std::is_fundamental_v<T>) {
            return __atomic_load_n(
                std::addressof(value_),
                atomic_detail::native_order<
                    __ATOMIC_RELAXED,
                    __ATOMIC_ACQUIRE,
                    __ATOMIC_CONSUME,
                    __ATOMIC_SEQ_CST>(order));
        } else {
            T result;
            __atomic_load(
                std::addressof(value_),
                std::addressof(result),
                atomic_detail::native_order<
                    __ATOMIC_RELAXED,
                    __ATOMIC_ACQUIRE,
                    __ATOMIC_CONSUME,
                    __ATOMIC_SEQ_CST>(order));
            return result;
        }
    }

    /**
     * Atomically loads and returns the current value of the atomic variable.
     *
     * Equivalent to load().
     */
    constexpr operator T () const noexcept { return load(); }

    /**
     * Atomically replaces the underlying value with @p desired (a
     * read-modify-write operation).
     *
     * Memory is affected according to the value of @p order.
     *
     * @pre  order is one of std::memory_order_relaxed,
     * std::memory_order_acquire, std::memory_order_release,
     * std::memory_order_acq_rel, or std::memory_order_seq_cst.
     */
    constexpr T exchange(
        T desired,
        std::memory_order order = std::memory_order_seq_cst) noexcept
    {
        if constexpr (std::is_fundamental_v<T>) {
            return __atomic_exchange_n(
                std::addressof(value_),
                desired,
                atomic_detail::native_order<
                    __ATOMIC_RELAXED,
                    __ATOMIC_ACQUIRE,
                    __ATOMIC_RELEASE,
                    __ATOMIC_ACQ_REL,
                    __ATOMIC_SEQ_CST>(order));
        } else {
            T result;
            __atomic_exchange(
                std::addressof(value_),
                std::addressof(desired),
                std::addressof(result),
                atomic_detail::native_order<
                    __ATOMIC_RELAXED,
                    __ATOMIC_ACQUIRE,
                    __ATOMIC_RELEASE,
                    __ATOMIC_ACQ_REL,
                    __ATOMIC_SEQ_CST>(order));
            return result;
        }
    }

    /**
     * Try to perform an exchange operation.
     *
     * Atomically compares the value representation of *this with that of @p
     * expected. If those are bitwise-equal, replaces the former with @p desired
     * (performs read-modify-write operation). Otherwise, loads the actual value
     * stored in *this into @p expected (performs load operation).
     *
     * @return  true if the underlying atomic value was successfully changed,
     * false otherwise.
     *
     * @see std::atomic for all the special considerations, as they are the
     * same.
     */
    constexpr bool compare_exchange_weak(
        T & expected,
        T desired,
        std::memory_order success,
        std::memory_order fail) noexcept
    {
        return compare_exchange_impl(expected, desired, true, success, fail);
    }

    /**
     * Try to perform an exchange operation.
     *
     * Atomically compares the value representation of *this with that of @p
     * expected. If those are bitwise-equal, replaces the former with @p desired
     * (performs read-modify-write operation). Otherwise, loads the actual value
     * stored in *this into @p expected (performs load operation).
     *
     * @return  true if the underlying atomic value was successfully changed,
     * false otherwise.
     *
     * @see std::atomic for all the special considerations, as they are the
     * same.
     */
    constexpr bool compare_exchange_weak(
        T & expected,
        T desired,
        std::memory_order order = std::memory_order_seq_cst) noexcept
    {
        return compare_exchange_impl(expected, desired, true, order, order);
    }

    /**
     * Try to perform an exchange operation.
     *
     * Atomically compares the value representation of *this with that of @p
     * expected. If those are bitwise-equal, replaces the former with @p desired
     * (performs read-modify-write operation). Otherwise, loads the actual value
     * stored in *this into @p expected (performs load operation).
     *
     * @return  true if the underlying atomic value was successfully changed,
     * false otherwise.
     *
     * @see std::atomic for all the special considerations, as they are the
     * same.
     */
    constexpr bool compare_exchange_strong(
        T & expected,
        T desired,
        std::memory_order success,
        std::memory_order fail) noexcept
    {
        return compare_exchange_impl(expected, desired, false, success, fail);
    }

    /**
     * Try to perform an exchange operation.
     *
     * Atomically compares the value representation of *this with that of @p
     * expected. If those are bitwise-equal, replaces the former with @p desired
     * (performs read-modify-write operation). Otherwise, loads the actual value
     * stored in *this into @p expected (performs load operation).
     *
     * @return  true if the underlying atomic value was successfully changed,
     * false otherwise.
     *
     * @see std::atomic for all the special considerations, as they are the
     * same.
     */
    constexpr bool compare_exchange_strong(
        T & expected,
        T desired,
        std::memory_order order = std::memory_order_seq_cst) noexcept
    {
        return compare_exchange_impl(expected, desired, false, order, order);
    }

    /**
     * Atomically replaces the current value with the result of arithmetic
     * addition of the value and @p arg.
     *
     * That is, it performs atomic post-increment. The operation is a
     * read-modify-write operation. Memory is affected according to the value of
     * @p order.
     *
     * @return  The value immediately preceding the effects of this function in
     * the modification order of *this.
     *
     * @note  Participates in overload resolution when T is a float or integral
     * (except bool).
     */
    constexpr T fetch_add(
        T arg,
        std::memory_order order = std::memory_order_seq_cst) noexcept
    requires atomic_detail::integral<T> || std::floating_point<T>
    {
        return __atomic_fetch_add(
            std::addressof(value_),
            arg,
            static_cast<Order>(order));
    }

    /**
     * Atomically replaces the current value with the result of arithmetic
     * addition of the value and @p arg.
     *
     * That is, it performs atomic post-increment. The operation is a
     * read-modify-write operation. Memory is affected according to the value of
     * @p order.
     *
     * @return  The value immediately preceding the effects of this function in
     * the modification order of *this.
     *
     * @note  Participates in overload resolution when T is an object pointer.
     */
    constexpr T fetch_add(
        std::ptrdiff_t arg,
        std::memory_order order = std::memory_order_seq_cst) noexcept
    requires atomic_detail::object_pointer<T>
    {
        return __atomic_fetch_add(
            std::addressof(value_),
            arg,
            static_cast<Order>(order));
    }

    /**
     * Atomically replaces the current value with the result of arithmetic
     * subtraction of the value and @p arg.
     *
     * That is, it performs atomic post-decrement. The operation is a
     * read-modify-write operation. Memory is affected according to the value of
     * @p order.
     *
     * @return The value immediately preceding the effects of this function in
     * the modification order of *this.
     *
     * @note  Participates in overload resolution when T is a float or integral
     * (except bool).
     */
    constexpr T fetch_sub(
        T arg,
        std::memory_order order = std::memory_order_seq_cst) noexcept
    requires atomic_detail::integral<T> || std::floating_point<T>
    {
        return __atomic_fetch_sub(
            std::addressof(value_),
            arg,
            static_cast<Order>(order));
    }

    /**
     * Atomically replaces the current value with the result of arithmetic
     * subtraction of the value and @p arg.
     *
     * That is, it performs atomic post-decrement. The operation is a
     * read-modify-write operation. Memory is affected according to the value of
     * @p order.
     *
     * @return The value immediately preceding the effects of this function in
     * the modification order of *this.
     *
     * @note  Participates in overload resolution when T is an object pointer.
     */
    constexpr T fetch_sub(
        std::ptrdiff_t arg,
        std::memory_order order = std::memory_order_seq_cst) noexcept
    requires atomic_detail::object_pointer<T>
    {
        return __atomic_fetch_sub(
            std::addressof(value_),
            arg,
            static_cast<Order>(order));
    }

    /**
     * Equivalent to return fetch_add(arg) + arg;.
     *
     * @note  Unlike most assignment operators, the assignment operators for
     * atomic types do not return a reference to their left-hand arguments. They
     * return a copy of the stored value instead.
     *
     * @note  Participates in overload resolution when T is a float or integral
     * (except bool).
     */
    constexpr T operator += (T arg) noexcept
    requires atomic_detail::integral<T> || std::floating_point<T>
    {
        return fetch_add(arg) + arg;
    }

    /**
     * Equivalent to return fetch_add(arg) + arg;.
     *
     * @note  Unlike most assignment operators, the assignment operators for
     * atomic types do not return a reference to their left-hand arguments. They
     * return a copy of the stored value instead.
     *
     * @note  Participates in overload resolution when T is an object pointer.
     */
    constexpr T operator += (std::ptrdiff_t arg) noexcept
    requires atomic_detail::object_pointer<T>
    {
        return fetch_add(arg) + arg;
    }

    /**
     * Equivalent to return fetch_add(arg) - arg;.
     *
     * @note  Unlike most assignment operators, the assignment operators for
     * atomic types do not return a reference to their left-hand arguments. They
     * return a copy of the stored value instead.
     *
     * @note  Participates in overload resolution when T is a float or integral
     * (except bool).
     */
    constexpr T operator -= (T arg) noexcept
    requires atomic_detail::integral<T> || std::floating_point<T>
    {
        return fetch_sub(arg) - arg;
    }

    /**
     * Equivalent to return fetch_add(arg) - arg;.
     *
     * @note  Unlike most assignment operators, the assignment operators for
     * atomic types do not return a reference to their left-hand arguments. They
     * return a copy of the stored value instead.
     *
     * @note  Participates in overload resolution when T is an object pointer.
     */
    constexpr T operator -= (std::ptrdiff_t arg) noexcept
    requires atomic_detail::object_pointer<T>
    {
        return fetch_sub(arg) - arg;
    }

    /**
     * Performs atomic pre-increment.
     *
     * Equivalent to return fetch_add(1) + 1.
     *
     * @note  Unlike most assignment operators, the assignment operators for
     * atomic types do not return a reference to their left-hand arguments. They
     * return a copy of the stored value instead.
     *
     * @note  Participates in overload resolution when T is an object pointer or
     * integral (except bool).
     */
    constexpr T operator ++ () noexcept
    requires atomic_detail::integral<T> || atomic_detail::object_pointer<T>
    {
        using D = typename base::difference_type;
        return fetch_add(D(1)) + D(1);
    }

    /**
     * Performs atomic post-increment.
     *
     * Equivalent to return fetch_add(1).
     *
     * @note  Unlike most assignment operators, the assignment operators for
     * atomic types do not return a reference to their left-hand arguments. They
     * return a copy of the stored value instead.
     *
     * @note  Participates in overload resolution when T is an object pointer or
     * integral (except bool).
     */
    constexpr T operator ++ (int) noexcept
    requires atomic_detail::integral<T> || atomic_detail::object_pointer<T>
    {
        using D = typename base::difference_type;
        return fetch_add(D(1));
    }

    /**
     * Performs atomic pre-decrement.
     *
     * Equivalent to return fetch_sub(1) - 1.
     *
     * @note  Unlike most assignment operators, the assignment operators for
     * atomic types do not return a reference to their left-hand arguments. They
     * return a copy of the stored value instead.
     *
     * @note  Participates in overload resolution when T is an object pointer or
     * integral (except bool).
     */
    constexpr T operator -- () noexcept
    requires atomic_detail::integral<T> || atomic_detail::object_pointer<T>
    {
        using D = typename base::difference_type;
        return fetch_sub(D(1)) - D(1);
    }

    /**
     * Performs atomic post-decrement.
     *
     * Equivalent to return fetch_sub(1).
     *
     * @note  Unlike most assignment operators, the assignment operators for
     * atomic types do not return a reference to their left-hand arguments. They
     * return a copy of the stored value instead.
     *
     * @note  Participates in overload resolution when T is an object pointer or
     * integral (except bool).
     */
    constexpr T operator -- (int) noexcept
    requires atomic_detail::integral<T> || atomic_detail::object_pointer<T>
    {
        using D = typename base::difference_type;
        return fetch_sub(D(1));
    }

    /**
     * Atomically replaces the current value with the result of bitwise AND of
     * the value and @p arg.
     *
     * The operation is read-modify-write operation. Memory is affected
     * according to the value of @p order.
     *
     * @return  The value immediately preceding the effects of this function in
     * the modification order of *this.
     *
     * @note  Participates in overload resolution when T is an integral (except
     * bool).
     */
    constexpr T fetch_and(
        T arg,
        std::memory_order order = std::memory_order_seq_cst) noexcept
    requires atomic_detail::integral<T>
    {
        return __atomic_fetch_and(
            std::addressof(value_),
            arg,
            static_cast<Order>(order));
    }

    /**
     * Atomically replaces the current value with the result of bitwise OR of
     * the value and @p arg.
     *
     * The operation is read-modify-write operation. Memory is affected
     * according to the value of @p order.
     *
     * @return  The value immediately preceding the effects of this function in
     * the modification order of *this.
     *
     * @note  Participates in overload resolution when T is an integral (except
     * bool).
     */
    constexpr T fetch_or(
        T arg,
        std::memory_order order = std::memory_order_seq_cst) noexcept
    requires atomic_detail::integral<T>
    {
        return __atomic_fetch_or(
            std::addressof(value_),
            arg,
            static_cast<Order>(order));
    }

    /**
     * Atomically replaces the current value with the result of bitwise XOR of
     * the value and @p arg.
     *
     * The operation is read-modify-write operation. Memory is affected
     * according to the value of @p order.
     *
     * @return  The value immediately preceding the effects of this function in
     * the modification order of *this.
     *
     * @note  Participates in overload resolution when T is an integral (except
     * bool).
     */
    constexpr T fetch_xor(
        T arg,
        std::memory_order order = std::memory_order_seq_cst) noexcept
    requires atomic_detail::integral<T>
    {
        return __atomic_fetch_xor(
            std::addressof(value_),
            arg,
            static_cast<Order>(order));
    }

    /**
     * Performs atmoic bitwise and.
     *
     * Equivalent tp fetch_and(arg) & arg.
     *
     * @note  Unlike most assignment operators, the assignment operators for
     * atomic types do not return a reference to their left-hand arguments. They
     * return a copy of the stored value instead.
     *
     * @note  Participates in overload resolution when T is an integral (except
     * bool).
     */
    constexpr T operator &= (T arg) noexcept
    requires atomic_detail::integral<T>
    {
        return fetch_and(arg) & arg;
    }

    /**
     * Performs atmoic bitwise or.
     *
     * Equivalent tp fetch_and(arg) | arg.
     *
     * @note  Unlike most assignment operators, the assignment operators for
     * atomic types do not return a reference to their left-hand arguments. They
     * return a copy of the stored value instead.
     *
     * @note  Participates in overload resolution when T is an integral (except
     * bool).
     */
    constexpr T operator |= (T arg) noexcept
    requires atomic_detail::integral<T>
    {
        return fetch_or(arg) | arg;
    }

    /**
     * Performs atmoic bitwise xor.
     *
     * Equivalent tp fetch_and(arg) ^ arg.
     *
     * @note  Unlike most assignment operators, the assignment operators for
     * atomic types do not return a reference to their left-hand arguments. They
     * return a copy of the stored value instead.
     *
     * @note  Participates in overload resolution when T is an integral (except
     * bool).
     */
    constexpr T operator ^= (T arg) noexcept
    requires atomic_detail::integral<T>
    {
        return fetch_xor(arg) ^ arg;
    }

    /**
     * Initializes the default-constructed atomic object obj with the value
     * desired. The function is not atomic: concurrent access from another
     * thread, even through an atomic operation, is a data race.
     *
     * @note  If obj was not default-constructed, or this function is called
     * twice on the same obj, the behavior is undefined.
     */
    static constexpr void init(Atomic & obj, value_type desired) noexcept
    {
        obj.value_ = desired;
    }

private:
    // Not sure how this could be false unless we messed up our requirements
    static_assert(std::is_nothrow_default_constructible_v<T>);
    using Order = atomic_detail::Order;

    constexpr bool compare_exchange_impl(
        T & expected,
        T desired,
        bool weak,
        std::memory_order success,
        std::memory_order fail) noexcept
    {
        // There are no restrictions on what memory order can be used here
        auto const sorder = static_cast<Order>(success);

        // It also cannot be a stronger order than that specified by
        // success_memorder - not sure what that means
        auto const forder = atomic_detail::native_order<
            __ATOMIC_RELAXED,
            __ATOMIC_CONSUME,
            __ATOMIC_ACQUIRE,
            __ATOMIC_SEQ_CST>(fail);

        // It also cannot be a stronger order than that specified by success
        assert(sorder >= forder);

        if constexpr (std::is_fundamental_v<T>) {
            return __atomic_compare_exchange_n(
                std::addressof(value_),
                std::addressof(expected),
                desired,
                weak,
                sorder,
                forder);
        } else {
            return __atomic_compare_exchange(
                std::addressof(value_),
                std::addressof(expected),
                std::addressof(desired),
                weak,
                sorder,
                forder);
        }
    }
};

template <typename T>
using ipc_atomic = Atomic<T>;

/**
 * Initializes a default constructed ipc_atomic object.
 *
 * @param obj  Pointer to the object to initialize
 * @param desired  The value the object will be initialized with
 *
 * @note  If obj was not default-constructed, or this function is called twice
 * on the same obj, the behavior is undefined.
 */
template <typename T>
constexpr void
atomic_init(
    ipc_atomic<T> * obj,
    typename ipc_atomic<T>::value_type desired) noexcept
{
    assert(obj);
    ipc_atomic<T>::init(*obj, desired);
}

using ipc_atomic_bool = ipc_atomic<bool>;
using ipc_atomic_char = ipc_atomic<char>;
using ipc_atomic_schar = ipc_atomic<signed char>;
using ipc_atomic_uchar = ipc_atomic<unsigned char>;
using ipc_atomic_short = ipc_atomic<short>;
using ipc_atomic_ushort = ipc_atomic<unsigned short>;
using ipc_atomic_int = ipc_atomic<int>;
using ipc_atomic_uint = ipc_atomic<unsigned int>;
using ipc_atomic_long = ipc_atomic<long>;
using ipc_atomic_ulong = ipc_atomic<unsigned long>;
using ipc_atomic_llong = ipc_atomic<long long>;
using ipc_atomic_ullong = ipc_atomic<unsigned long long>;
using ipc_atomic_char8_t = ipc_atomic<char8_t>;
using ipc_atomic_char16_t = ipc_atomic<char16_t>;
using ipc_atomic_char32_t = ipc_atomic<char32_t>;
using ipc_atomic_wchar_t = ipc_atomic<wchar_t>;

using ipc_atomic_int_least8_t = ipc_atomic<std::int_least8_t>;
using ipc_atomic_uint_least8_t = ipc_atomic<std::uint_least8_t>;
using ipc_atomic_int_least16_t = ipc_atomic<std::int_least16_t>;
using ipc_atomic_uint_least16_t = ipc_atomic<std::uint_least16_t>;
using ipc_atomic_int_least32_t = ipc_atomic<std::int_least32_t>;
using ipc_atomic_uint_least32_t = ipc_atomic<std::uint_least32_t>;
using ipc_atomic_int_least64_t = ipc_atomic<std::int_least64_t>;
using ipc_atomic_uint_least64_t = ipc_atomic<std::uint_least64_t>;

using ipc_atomic_int_fast8_t = ipc_atomic<std::int_fast8_t>;
using ipc_atomic_uint_fast8_t = ipc_atomic<std::uint_fast8_t>;
using ipc_atomic_int_fast16_t = ipc_atomic<std::int_fast16_t>;
using ipc_atomic_uint_fast16_t = ipc_atomic<std::uint_fast16_t>;
using ipc_atomic_int_fast32_t = ipc_atomic<std::int_fast32_t>;
using ipc_atomic_uint_fast32_t = ipc_atomic<std::uint_fast32_t>;
using ipc_atomic_int_fast64_t = ipc_atomic<std::int_fast64_t>;
using ipc_atomic_uint_fast64_t = ipc_atomic<std::uint_fast64_t>;

using ipc_atomic_int8_t = ipc_atomic<std::int8_t>;
using ipc_atomic_uint8_t = ipc_atomic<std::uint8_t>;
using ipc_atomic_int16_t = ipc_atomic<std::int16_t>;
using ipc_atomic_uint16_t = ipc_atomic<std::uint16_t>;
using ipc_atomic_int32_t = ipc_atomic<std::int32_t>;
using ipc_atomic_uint32_t = ipc_atomic<std::uint32_t>;
using ipc_atomic_int64_t = ipc_atomic<std::int64_t>;
using ipc_atomic_uint64_t = ipc_atomic<std::uint64_t>;

using ipc_atomic_intptr_t = ipc_atomic<std::intptr_t>;
using ipc_atomic_uintptr_t = ipc_atomic<std::uintptr_t>;
using ipc_atomic_size_t = ipc_atomic<std::size_t>;
using ipc_atomic_ptrdiff_t = ipc_atomic<std::ptrdiff_t>;
using ipc_atomic_intmax_t = ipc_atomic<std::intmax_t>;
using ipc_atomic_uintmax_t = ipc_atomic<std::uintmax_t>;

} // namespace wjh

#endif // WJH_8176d027a70b4e488f5aaf7a4492587e
