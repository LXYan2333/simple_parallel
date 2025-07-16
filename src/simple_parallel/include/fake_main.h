#pragma once

#include <simple_parallel/cxx/lib_visibility.h>

namespace simple_parallel {

using main_fn_t = int (*)(int, char **, char **);

auto fake_main(int argc, char **argv, char **envp, main_fn_t real_main) -> int;

} // namespace simple_parallel

// NOLINTNEXTLINE(*-cpp,cert-dcl37-c,*reserved-identifier)
extern "C" auto __real_main(int argc, char **argv, char **envp) -> int;

// NOLINTNEXTLINE(*-cpp,cert-dcl37-c,*reserved-identifier)
extern "C" S_P_LIB_PUBLIC auto __wrap_main(int argc, char **argv, char **envp)
    -> int;
