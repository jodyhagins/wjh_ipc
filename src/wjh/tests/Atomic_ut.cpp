// ======================================================================
// Copyright 2025 Jody Hagins
// Distributed under the MIT Software License
// See accompanying file LICENSE or copy at
// https://opensource.org/licenses/MIT
// ======================================================================
#include "wjh/Atomic.hpp"

#include "doctest.hpp"
#include "rapidcheck.hpp"

#include <chrono>
#include <latch>
#include <numeric>
#include <thread>
#include <vector>

namespace {
using wjh::Atomic;

template <typename T>
using nested_difference_type = typename T::difference_type;

template <template <typename...> class Tmpl, typename... Ts>
inline constexpr bool is_valid = requires { typename Tmpl<Ts...>::value_type; };

unsigned
run_n_threads(unsigned n, auto fn, auto... args)
{
    auto started = std::latch{n};
    auto go = std::latch{1};
    std::vector<std::thread> threads;
    for (unsigned i = 0; i < n; ++i) {
        threads.emplace_back([=, &started, &go] {
            started.count_down();
            go.wait();
            fn(args...);
        });
    }
    started.wait();
    go.count_down();
    for (auto & t : threads) {
        t.join();
    }
    return n;
};

unsigned
run_threads(auto fn, auto... args)
{
    return run_n_threads(
        std::thread::hardware_concurrency() / 2 + 1,
        std::move(fn),
        std::move(args)...);
}

TEST_SUITE("Atomic")
{
    struct T
    {
        int x;
    };

    static_assert(std::is_trivially_default_constructible_v<T>);
    static_assert(is_valid<Atomic, T>);

    TEST_CASE("difference_type")
    {
        SUBCASE("integral")
        {
            CHECK(std::is_same_v<int, Atomic<int>::difference_type>);
        }
        SUBCASE("floating point")
        {
            CHECK(std::is_same_v<float, Atomic<float>::difference_type>);
        }
        SUBCASE("pointer")
        {
            CHECK(
                std::is_same_v<std::ptrdiff_t, Atomic<int *>::difference_type>);
        }
        SUBCASE("bool")
        {
            CHECK(not is_valid<nested_difference_type, Atomic<bool>>);
        }
        SUBCASE("T")
        {
            CHECK(not is_valid<nested_difference_type, Atomic<T>>);
        }
    }

    TEST_CASE("value_type")
    {
        SUBCASE("integral")
        {
            CHECK(std::is_same_v<int, Atomic<int>::value_type>);
        }
        SUBCASE("floating point")
        {
            CHECK(std::is_same_v<float, Atomic<float>::value_type>);
        }
        SUBCASE("pointer")
        {
            CHECK(std::is_same_v<int *, Atomic<int *>::value_type>);
        }
        SUBCASE("bool")
        {
            CHECK(std::is_same_v<bool, Atomic<bool>::value_type>);
        }
        SUBCASE("T")
        {
            CHECK(std::is_same_v<T, Atomic<T>::value_type>);
        }
    }

    TEST_CASE("Atomic<int> is trivially default constructible")
    {
        CHECK(is_valid<Atomic, int>);
        CHECK(std::is_trivially_default_constructible_v<Atomic<int>>);
        CHECK(not std::is_copy_constructible_v<Atomic<int>>);
        CHECK(not std::is_copy_assignable_v<Atomic<int>>);
        CHECK(not std::is_move_constructible_v<Atomic<int>>);
        CHECK(not std::is_move_assignable_v<Atomic<int>>);
        CHECK(std::is_trivially_destructible_v<Atomic<int>>);
        int storage = 99;
        auto a = ::new (static_cast<void *>(&storage)) Atomic<int>;
        CHECK(99 == a->load());
    }
    TEST_CASE("Atomic<int> is default brace initialized to zero")
    {
        int storage = 42;
        auto a = ::new (static_cast<void *>(&storage)) Atomic<int>{};
        CHECK(0 == a->load());
        *a = 99;
        a = ::new (static_cast<void *>(&storage)) Atomic<int>();
        CHECK(0 == a->load());
    }
    TEST_CASE("Atomic<int> is initialized to value")
    {
        int storage = 42;
        auto a = ::new (static_cast<void *>(&storage)) Atomic<int>{86};
        CHECK(86 == a->load());
        a = ::new (static_cast<void *>(&storage)) Atomic<int>(99);
        CHECK(99 == a->load());
    }

    TEST_CASE("Can instantiate with trivially constructible type")
    {
        CHECK(std::is_trivially_default_constructible_v<Atomic<T>>);
        CHECK(not std::is_copy_constructible_v<Atomic<T>>);
        CHECK(not std::is_copy_assignable_v<Atomic<T>>);
        CHECK(not std::is_move_constructible_v<Atomic<T>>);
        CHECK(not std::is_move_assignable_v<Atomic<T>>);
        CHECK(std::is_trivially_destructible_v<Atomic<T>>);
        CHECK(is_valid<Atomic, T>);
        CHECK(std::is_trivially_default_constructible_v<Atomic<T>>);
        T storage{42};
        auto a = ::new (static_cast<void *>(&storage)) Atomic<T>;
        CHECK(T{42}.x == a->load().x);
    }
    TEST_CASE("T is default brace initialized to zero")
    {
        T storage{42};
        auto a = ::new (static_cast<void *>(&storage)) Atomic<T>{};
        CHECK(0 == a->load().x);
        *a = {99};
        a = ::new (static_cast<void *>(&storage)) Atomic<T>();
        CHECK(0 == a->load().x);
    }
    TEST_CASE("T is initialized to value")
    {
        T storage{42};
        auto a = ::new (static_cast<void *>(&storage)) Atomic<T>{{86}};
        CHECK(86 == a->load().x);
        a = ::new (static_cast<void *>(&storage)) Atomic<T>({99});
        CHECK(99 == a->load().x);
    }

    TEST_CASE("Can't instantiate with non-trivially constructible type")
    {
        struct U
        {
            int x = 42;
        };

        static_assert(not std::is_trivially_default_constructible_v<U>);
        CHECK(not is_valid<Atomic, U>);
    }

    TEST_CASE("basic load/store")
    {
        Atomic<int> x(0);
        CHECK(x.load() == 0);

        x.store(42);
        CHECK(x.load() == 42);
        CHECK(static_cast<int>(x) == 42);

        x.store(123, std::memory_order_relaxed);
        CHECK(x.load(std::memory_order_relaxed) == 123);
    }

    TEST_CASE("struct load/store")
    {
        Atomic<T> x{};
        CHECK(x.load().x == 0);

        x.store({42});
        CHECK(x.load().x == 42);

        x.store({123}, std::memory_order_relaxed);
        CHECK(x.load(std::memory_order_relaxed).x == 123);
    }

    TEST_CASE("basic exchange")
    {
        Atomic<int> x(10);

        int old = x.exchange(20);
        CHECK(old == 10);
        CHECK(x.load() == 20);

        old = x.exchange(999, std::memory_order_relaxed);
        CHECK(old == 20);
        CHECK(x.load() == 999);
    }

    TEST_CASE("struct exchange")
    {
        Atomic<T> x{{10}};

        auto old = x.exchange({20});
        CHECK(old.x == 10);
        CHECK(x.load().x == 20);

        old = x.exchange({999}, std::memory_order_relaxed);
        CHECK(old.x == 20);
        CHECK(x.load().x == 999);
    }

    TEST_CASE("basic compare_exchange_weak")
    {
        Atomic<int> x(100);

        int expected = 100;
        bool exchanged = x.compare_exchange_weak(expected, 200);
        CHECK(exchanged);
        CHECK(x.load() == 200);
        CHECK(expected == 100);

        expected = 300;
        exchanged = x.compare_exchange_weak(expected, 400);
        CHECK(not exchanged);
        CHECK(x.load() == 200);
        CHECK(expected == 200);
    }

    TEST_CASE("T compare_exchange_weak")
    {
        Atomic<T> x{{100}};

        T expected = {100};
        bool exchanged = x.compare_exchange_weak(expected, {200});
        CHECK(exchanged);
        CHECK(x.load().x == 200);
        CHECK(expected.x == 100);

        expected = {300};
        exchanged = x.compare_exchange_weak(expected, {400});
        CHECK(not exchanged);
        CHECK(x.load().x == 200);
        CHECK(expected.x == 200);
    }

    TEST_CASE("basic compare_exchange_strong")
    {
        Atomic<int> x(100);

        int expected = 100;
        bool exchanged = x.compare_exchange_strong(expected, 200);
        CHECK(exchanged);
        CHECK(x.load() == 200);
        CHECK(expected == 100);

        expected = 300;
        exchanged = x.compare_exchange_strong(expected, 400);
        CHECK(not exchanged);
        CHECK(x.load() == 200);
        CHECK(expected == 200);
    }

    TEST_CASE("T compare_exchange_strong")
    {
        Atomic<T> x{{100}};

        T expected = {100};
        bool exchanged = x.compare_exchange_strong(expected, {200});
        CHECK(exchanged);
        CHECK(x.load().x == 200);
        CHECK(expected.x == 100);

        expected = {300};
        exchanged = x.compare_exchange_strong(expected, {400});
        CHECK(not exchanged);
        CHECK(x.load().x == 200);
        CHECK(expected.x == 200);
    }

    TEST_CASE("fetch_add")
    {
        Atomic<int> counter(0);

        int old = counter.fetch_add(5);
        CHECK(old == 0);
        CHECK(counter.load() == 5);

        old = counter.fetch_add(3);
        CHECK(old == 5);
        CHECK(counter.load() == 8);
    }

    TEST_CASE("fetch_add pointer")
    {
        auto const orig = std::array<char, 10>{"abcdefghi"};
        Atomic<char const *> p(orig.data());

        auto old = p.fetch_add(5);
        CHECK(old == &orig.at(0));
        CHECK(p.load() == &orig.at(5));

        old = p.fetch_add(3);
        CHECK(old == &orig.at(5));
        CHECK(p.load() == &orig.at(8));
    }

    TEST_CASE("fetch_sub")
    {
        Atomic<int> counter(0);

        int old = counter.fetch_sub(5);
        CHECK(old == 0);
        CHECK(counter.load() == -5);

        old = counter.fetch_sub(3);
        CHECK(old == -5);
        CHECK(counter.load() == -8);
    }

    TEST_CASE("fetch_sub pointer")
    {
        auto const orig = std::array<char, 10>{"abcdefghi"};
        Atomic<char const *> p(&orig.at(8));

        auto old = p.fetch_sub(5);
        CHECK(old == &orig.at(8));
        CHECK(p.load() == &orig.at(3));

        old = p.fetch_sub(3);
        CHECK(old == &orig.at(3));
        CHECK(p.load() == &orig.at(0));
    }

    TEST_CASE("concurrent compare_exchange")
    {
        auto check = [](std::vector<std::int32_t> const & input) {
            RC_PRE(input.size() > 10);

            Atomic<std::int64_t> counter(0);
            auto execute = [&] {
                for (std::size_t k = 0; k < input.size(); ++k) {
                    if (input[k] != 0) {
                        while (true) {
                            auto oldval = counter.load();
                            auto newval = oldval + input[k];
                            if (counter.compare_exchange_strong(oldval, newval))
                            {
                                break;
                            }
                        }
                    }
                }
            };

            auto const expected =
                std::accumulate(input.begin(), input.end(), 0ll);
            auto n = run_threads(execute);
            RC_ASSERT(counter.load() == expected * n);
        };

        using Input = std::vector<std::vector<std::vector<std::int32_t>>>;
        rc::doctest::check([&](Input const & input) {
            std::vector<std::int32_t> all;
            for (auto const & v1 : input) {
                for (auto const & v2 : v1) {
                    all.insert(all.end(), v2.begin(), v2.end());
                }
            }
            check(all);
        });
    }

    TEST_CASE("Concurrent Increment and Decrement")
    {
        auto x = std::atomic<unsigned>{};
        auto y = wjh::Atomic<unsigned>{};
        auto exec = [](auto & arg) {
            arg.fetch_add(7);
            arg.fetch_sub(3);
            ++arg;
            arg++;
            --arg;
            arg--;
            arg += 13;
            arg -= 11;
        };
        run_threads([&] {
            auto count = 1'000'000;
            while (count--) {
                exec(x);
                exec(y);
            }
        });
        CHECK(x.load() == y.load());
    }

    TEST_CASE("load and store with memory orders")
    {
        Atomic<int> a(0);

        // For example, store using memory_order_relaxed
        a.store(10, std::memory_order_relaxed);
        CHECK(a.load(std::memory_order_relaxed) == 10);

        // store using memory_order_release, load using memory_order_acquire
        a.store(20, std::memory_order_release);
        int value = a.load(std::memory_order_acquire);
        CHECK(value == 20);
    }

    TEST_CASE("exchange")
    {
        Atomic<int> a(5);

        int old = a.exchange(10);
        CHECK(old == 5);
        CHECK(a.load() == 10);

        old = a.exchange(15, std::memory_order_acq_rel);
        CHECK(old == 10);
        CHECK(a.load(std::memory_order_relaxed) == 15);
    }

    TEST_CASE("compare_exchange_strong")
    {
        Atomic<int> a(10);

        int expected = 10;
        bool success = a.compare_exchange_strong(expected, 20);
        CHECK(success);
        CHECK(a.load() == 20);

        expected = 10;
        success = a.compare_exchange_strong(expected, 30);
        CHECK_FALSE(success);
        CHECK(expected == 20);
        CHECK(a.load() == 20);

        expected = 20;
        success =
            a.compare_exchange_strong(expected, 40, std::memory_order_acquire);
        CHECK(success);
        CHECK(a.load(std::memory_order_relaxed) == 40);
    }

    TEST_CASE("compare_exchange_weak")
    {
        Atomic<int> a(5);

        int expected = 5;
        bool success = false;

        do {
            expected = 5;
            success = a.compare_exchange_weak(expected, 6);
        } while (not success && expected == 5);

        CHECK(a.load() == 6);
    }

    TEST_CASE("fetch_add/fetch_sub")
    {
        Atomic<int> a(0);

        int old = a.fetch_add(5);
        CHECK(old == 0);
        CHECK(a.load() == 5);

        old = a.fetch_sub(2, std::memory_order_relaxed);
        CHECK(old == 5);
        CHECK(a.load() == 3);
    }

    TEST_CASE("fetch_and/fetch_or/fetch_xor")
    {
        Atomic<unsigned int> a(0xFF);

        auto old = a.fetch_and(0x0F);
        CHECK(old == 0xFF);
        CHECK(a.load() == 0x0F);

        old = a.fetch_or(0xF0);
        CHECK(old == 0x0F);
        CHECK(a.load() == 0xFF);

        old = a.fetch_xor(0xAA);
        CHECK(old == 0xFF);
        CHECK(a.load() == (0xFF ^ 0xAA));
    }

    TEST_CASE("concurrent increment test")
    {
        Atomic<unsigned> counter(0);
        auto worker = [&](int increments) {
            for (int i = 0; i < increments; i++) {
                counter.fetch_add(1, std::memory_order_relaxed);
            }
        };
        auto n = run_threads(worker, 100000);
        CHECK(counter.load() == n * 100000);
    }

    TEST_CASE("concurrent CAS spin test")
    {
        Atomic<int> a(0);
        auto worker = [&](int num_iter) {
            for (int i = 0; i < num_iter; i++) {
                while (true) {
                    int expected = a.load(std::memory_order_relaxed);
                    if (a.compare_exchange_weak(
                            expected,
                            expected + 1,
                            std::memory_order_release,
                            std::memory_order_relaxed))
                    {
                        break;
                    }
                }
            }
        };
        auto n = run_threads(worker, 100000);
        CHECK(a.load(std::memory_order_acquire) == n * 100000);
    }

    TEST_CASE("load/store invariants")
    {
        rc::doctest::check(
            "Atomic<T> should store/load the same value",
            [](int x) {
                Atomic<int> a(0);
                a.store(x, std::memory_order_relaxed);
                int y = a.load(std::memory_order_relaxed);
                RC_ASSERT(x == y);
            });
    }

    TEST_CASE("exchange invariants")
    {
        rc::doctest::check(
            "Atomic<T> exchange returns the old value and sets the new",
            [](int init, int newVal) {
                Atomic<int> a(init);
                int oldVal = a.exchange(newVal, std::memory_order_relaxed);
                RC_ASSERT(oldVal == init);
                RC_ASSERT(a.load(std::memory_order_relaxed) == newVal);
            });
    }

    TEST_CASE("fetch_add invariants")
    {
        rc::doctest::check(
            "Atomic<T> fetch_add increments atomic by the given amount",
            [](int init, int inc) {
                Atomic<int> a(init);
                int oldVal = a.fetch_add(inc, std::memory_order_relaxed);
                RC_ASSERT(oldVal == init);
                RC_ASSERT(a.load(std::memory_order_relaxed) == (init + inc));
            });
    }
}

} // anonymous namespace
