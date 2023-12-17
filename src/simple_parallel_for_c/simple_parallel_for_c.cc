#include <simple_parallel_for_c/simple_parallel_for_c.h>

#include <internal_use_only/simple_parallel_config.h>
#include <simple_parallel/simple_parallel.h>

extern "C" {
    auto simple_parallel_init(int (*virtual_main)(int, char**),
                              int argc,
                              char** argv,
                              bool init_mpi) -> void {
        simple_parallel::init(virtual_main, argc, argv, init_mpi);
    }

    auto simple_parallel_run_lambda(void* lambda, bool parallel_run) -> void {
        simple_parallel::run_lambda(
            [&] {
#ifdef SIMPLE_PARALLEL_COMPILER_GNU
                // use gcc's nested function extension
                // see https://gcc.gnu.org/onlinedocs/gcc/Nested-Functions.html
                (*reinterpret_cast<void (*)()>(lambda))();
#else
                // use clang's block extension
                // see https://clang.llvm.org/docs/BlockLanguageSpec.html
                //     https://thephd.dev/lambdas-nested-functions-block-expressions-oh-my
                (*reinterpret_cast<void (^(*))(void)>(lambda))();
#endif
            },
            parallel_run);
    };
}
