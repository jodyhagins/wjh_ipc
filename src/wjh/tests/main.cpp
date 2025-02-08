// ======================================================================
// Copyright 2025 Jody Hagins
// Distributed under the MIT Software License
// See accompanying file LICENSE or copy at
// https://opensource.org/licenses/MIT
// ======================================================================

#if 1
    #define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
    #include "doctest.hpp"
#else
    #define DOCTEST_CONFIG_IMPLEMENT
    #include "doctest.hpp"

int
main(int argc, char ** argv)
{
    doctest::Context context;

    // Enable detailed output
    context.setOption("duration", true); // Print duration of each test

    context.applyCommandLine(argc, argv);
    return context.run();
}
#endif
