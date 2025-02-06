// ======================================================================
// Copyright 2025 Jody Hagins
// Distributed under the MIT Software License
// See accompanying file LICENSE or copy at
// https://opensource.org/licenses/MIT
// ======================================================================
#ifndef WJH_4f23ec6051c641cba8bb05dadfb94612
#define WJH_4f23ec6051c641cba8bb05dadfb94612

#include <atomic>
#include <cassert>
#include <concepts>
#include <type_traits>

namespace wjh::atomic_detail {

// We can't really implement this without help from the compiler, so this is
// just an estimate.
template <typename T>
concept user_provided_destructible = std::is_destructible_v<T> &&
    not std::is_trivially_destructible_v<T>;

// Not exactly correct, but close enough for current use.
template <typename T>
concept implicit_lifetime = (not user_provided_destructible<T> &&
                             (std::is_scalar_v<T> || std::is_array_v<T> ||
                              std::is_aggregate_v<T>)) ||
    (std::is_trivially_destructible_v<T> &&
     (std::is_trivially_default_constructible_v<T> ||
      std::is_trivially_copy_constructible_v<T> ||
      std::is_trivially_move_constructible_v<T>));

template <typename T>
concept integral = std::integral<T> &&
    not std::same_as<bool, std::remove_cv_t<T>>;

template <typename T>
concept object_pointer = std::is_pointer_v<T> &&
    not std::is_function_v<std::remove_pointer_t<T>>;

template <typename T>
struct atomic_base
{
    // No difference_type in the primary template.
};

template <typename T>
requires integral<T> || std::floating_point<T>
struct atomic_base<T>
{
    using difference_type = T;
};

template <typename T>
requires object_pointer<T>
struct atomic_base<T>
{
    using difference_type = std::ptrdiff_t;
};

using Order = decltype(__ATOMIC_RELAXED);
static_assert(
    __ATOMIC_RELAXED == static_cast<Order>(std::memory_order_relaxed));
static_assert(
    __ATOMIC_CONSUME == static_cast<Order>(std::memory_order_consume));
static_assert(
    __ATOMIC_ACQUIRE == static_cast<Order>(std::memory_order_acquire));
static_assert(
    __ATOMIC_RELEASE == static_cast<Order>(std::memory_order_release));
static_assert(
    __ATOMIC_ACQ_REL == static_cast<Order>(std::memory_order_acq_rel));
static_assert(
    __ATOMIC_SEQ_CST == static_cast<Order>(std::memory_order_seq_cst));

template <Order... Orders>
constexpr Order
native_order(std::memory_order order)
{
    assert(((static_cast<Order>(order) == Orders) || ...));
    return static_cast<Order>(order);
}

} // namespace wjh::atomic_detail

#endif // WJH_4f23ec6051c641cba8bb05dadfb94612
