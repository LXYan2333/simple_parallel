#pragma once

#include <internal_use_only/simple_parallel_config.h>

#ifdef SIMPLE_PARALLEL_COMPILER_GNU
// use gcc's nested function extension
// see https://gcc.gnu.org/onlinedocs/gcc/Nested-Functions.html
// warning: this extension is not thread-safe!
    #define SIMPLE_PARALLEL_LAMBDA(name, return_type, ...)                     \
        return_type name(__VA_ARGS__)

// this is unecceary for gcc, so leave it empty
    #define S_P_ASSIGN
#else
// use clang's block extension
// see https://clang.llvm.org/docs/BlockLanguageSpec.html
//     https://thephd.dev/lambdas-nested-functions-block-expressions-oh-my
    #define SIMPLE_PARALLEL_LAMBDA(name, return_type, ...)                     \
        return_type (^name)(__VA_ARGS__) = ^(__VA_ARGS__)
// if you need to assign a variable in a block, a __block type specifier is
// nessesary
    #define S_P_ASSIGN __block
#endif

// test whether simple_parallel was compiled with the same compiler as the
// current project
#ifdef SIMPLE_PARALLEL_COMPILER_GNU // gcc
    #if !__GNUC__
        #error                                                                 \
            "simple_parallel was compiled with clang (and block extension is used), but now you are using it in a project compiled with gcc (which supports nested function extension instead of block extension). Please recompile simple_parallel with gcc to switch to its gcc nested function extension support."
    #endif
#else // clang
    #if __GNUC__ && !__clang__ && !__INTEL_COMPILER
        #error                                                                 \
            "simple_parallel was compiled with gcc (and nested function extension is used), but now you are using it in a project compiled with clang (which supports block extension instead of nested function extension). Please recompile simple_parallel with clang to switch to its block extension support."
    #endif
#endif
