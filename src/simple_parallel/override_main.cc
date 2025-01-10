#include <fake_main.h>

namespace simple_parallel {
// NOLINTNEXTLINE(*-global-variables)
main_fn_t original_main = nullptr;
} // namespace simple_parallel

// NOLINTNEXTLINE(*-cpp,cert-dcl37-c,*reserved-identifier)
extern "C" auto __wrap___libc_start_main(simple_parallel::main_fn_t main,
                                         int argc, char **argv,
                                         simple_parallel::main_fn_t init,
                                         void (*fini)(), void (*rtld_fini)(),
                                         void *stack_end) -> int {
  simple_parallel::original_main = main;
  return __real___libc_start_main(simple_parallel::fake_main, argc, argv, init,
                                  fini, rtld_fini, stack_end);
}