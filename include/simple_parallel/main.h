#pragma once

#include <simple_parallel/simple_parallel.h>

auto virtual_main(int argc, char** argv) -> int;

auto main(int argc, char** argv) -> int {

    simple_parallel::init(virtual_main, argc, argv);

    return 0;
}

// evil macro to rename `main` to `virtual_main`, and unify the signature to
// `int virtual_main(int, char**)`
//
// if you'd like to avoid this evil macro, you can add
//
// #define SIMPLE_PARALLEL_DO_NOT_RENAME_MAIN
//
// before include this header, rename `main` to `virtual_main` and make sure
// its signature is `int virtual_main(int, char**)`
#ifndef SIMPLE_PARALLEL_DO_NOT_RENAME_MAIN
    #define main_with_0_parameter(...) virtual_main(int, char**)
// for 1 parameter, 'void' must be the first and only parameter if specified
    #define main_with_1_parameter(...) virtual_main(int argc, char** argv)
    #define main_with_2_parameters(...) virtual_main(__VA_ARGS__)
    #define GET_RENAME_MAIN_MACRO(_0, _1, _2, NAME, ...) NAME(_1, _2)
    #define main(...)                                                          \
        GET_RENAME_MAIN_MACRO(_0 __VA_OPT__(, )##__VA_ARGS__,                  \
                              main_with_2_parameters,                          \
                              main_with_1_parameter,                           \
                              main_with_0_parameter)
#endif
