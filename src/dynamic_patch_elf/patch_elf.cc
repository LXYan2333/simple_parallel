#include <cstdlib>
#include <dlfcn.h>
#include <fake_main.h>

// `__libc_start_main` is a weak symbol of glibc thus we can override it.
//
// NOLINTNEXTLINE(*-cpp,cert-dcl37-c,*reserved-identifier)
extern "C" auto __libc_start_main(simple_parallel::main_fn_t main, int argc,
                                  char **argv, simple_parallel::main_fn_t init,
                                  void (*fini)(), void (*rtld_fini)(),
                                  void *stack_end) -> int {

  simple_parallel::original_main = main;

  // NOLINTBEGIN(*reinterpret-cast*)
  auto orig_start_main_fn =
      reinterpret_cast<simple_parallel::glibc_start_main_fn_t>(
          dlsym(RTLD_NEXT, "__libc_start_main"));
  // NOLINTEND(*reinterpret-cast*)

  return orig_start_main_fn(simple_parallel::fake_main, argc, argv, init, fini,
                            rtld_fini, stack_end);
};