#include <internal_use_only/simple_parallel_config.h>
#include <simple_parallel/simple_parallel.h>

#include <simple_parallel_for_c/simple_parallel_for_c.h>


extern "C" {
    auto simple_parallel_init(int (*virtual_main)(int, char**),
                              int    argc,
                              char** argv) -> int {
        int ret{};
        simple_parallel::init([&] { ret = virtual_main(argc, argv); });
        return ret;
    }

    auto simple_parallel_run_invocable(void* invocable,
                                       bool  parallel_run) -> void {
        simple_parallel::run_invocable(
            [&] {
#ifdef COMPILER_SUPPORTS_NESTED_FUNCTIONS
                // use gcc's nested function extension
                // see https://gcc.gnu.org/onlinedocs/gcc/Nested-Functions.html
                (*reinterpret_cast<void (*)()>(invocable))();
#else
                // use clang's block extension
                // see https://clang.llvm.org/docs/BlockLanguageSpec.html
                //     https://thephd.dev/lambdas-nested-functions-block-expressions-oh-my
                (*reinterpret_cast<void (^(*))(void)>(invocable))();
#endif
            },
            parallel_run);
    }
}
