#pragma once

#include <simple_parallel_for_c/simple_parallel_for_c.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

    int virtual_main(int argc, char** argv);

    int main(int argc, char** argv) {

        simple_parallel_init(virtual_main, argc, argv);

        return 0;
    }

#ifdef __cplusplus
}
#endif

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
    #define main_with_0_parameter(...) virtual_main(int argc, char** argv)
// for 1 parameter, 'void' must be the first and only parameter if specified
    #define main_with_1_parameter(...) virtual_main(int argc, char** argv)
    #define main_with_2_parameters(_0, _1) virtual_main(_0, _1)
    #define GET_RENAME_MAIN_MACRO(_0, _1, _2, NAME, ...) NAME(_1, _2)
    #define main(...)                                                          \
        GET_RENAME_MAIN_MACRO(_0,                                              \
                              ##__VA_ARGS__,                                   \
                              main_with_2_parameters,                          \
                              main_with_1_parameter,                           \
                              main_with_0_parameter)
#endif
