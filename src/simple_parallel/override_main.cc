#include <cstdlib>
#include <fake_main.h>
#include <iostream>

// NOLINTNEXTLINE(*-cpp,cert-dcl37-c,*reserved-identifier)
extern "C" auto __wrap_main(int argc, char **argv, char **envp) -> int {

  static bool main_called = false;
  if (main_called) {
    std::cerr << "Error: `main()` can not be called recursively when "
                 "simple_parallel is used!\n";
    // NOLINTNEXTLINE(*-mt-unsafe)
    exit(1);
  }
  main_called = true;

  return simple_parallel::fake_main(argc, argv, envp, __real_main);
}