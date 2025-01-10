#pragma once

namespace simple_parallel {

using main_fn_t = int (*)(int, char **, char **);

using glibc_start_main_fn_t = int (*)(main_fn_t, int, char **, main_fn_t,
                                      void (*)(), void (*)(), void *);

auto fake_main(int argc, char **argv, char **env) -> int;

// NOLINTNEXTLINE(*-global-variables)
extern main_fn_t original_main;

} // namespace simple_parallel

// NOLINTNEXTLINE(*-cpp,cert-dcl37-c,*reserved-identifier)
extern "C" auto __real___libc_start_main(simple_parallel::main_fn_t main,
                                         int argc, char **argv,
                                         simple_parallel::main_fn_t init,
                                         void (*fini)(), void (*rtld_fini)(),
                                         void *stack_end) -> int;

// NOLINTNEXTLINE(*-cpp,cert-dcl37-c,*reserved-identifier)
extern "C" auto __wrap___libc_start_main(simple_parallel::main_fn_t main,
                                         int argc, char **argv,
                                         simple_parallel::main_fn_t init,
                                         void (*fini)(), void (*rtld_fini)(),
                                         void *stack_end) -> int;
