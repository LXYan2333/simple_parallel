# drafts

switch off aslr: `echo 0 | sudo tee /proc/sys/kernel/randomize_va_space`

or better without sudo: `mpirun -np 2 setarch x86_64 -R`

```console
$ mpirun -np 2 setarch `uname -m` -R ./simple_parallel_example_for_c
```

there is a `S_P_ASSIGN` macro for easier portable programming in the c interface.

C array must decay to pointer before captured by block

block runtime library must be installed if clang compiler is used:

on debian: `libblocksruntime-dev/`

on gentoo: `sys-libs/blocksruntime`

or the linker will complain about missing `-lBlocksRuntime`.

~~mpi must be compiled with cpp bingding enabled (may change in the future).~~

global variables will not be synced.

BigCount (i.e. Embiggenment) MPI 4.0 standard specification currently.

# todo: 

- aslr check [done]

- memory layout check

- mpi bigcount check [ optional ]

- block runtime/nested function check [done] 

- verify the message sent to each rank is correct

- option to disable distributed-memory dynamic schedule [c done]

- master-in-place util function

- no need to use another heap, use default heap is enough. [done]

- special communicator in parallel section

- object target to override `malloc`s [important]

using ibcast [optional]

munmap

unix_madvise [done]

enviroment variable: SIMPLE_PARALLEL_DEBUG=1 [done]

- better dynamic schedule c api

- better dynamic schedule implementation (add another layer of buffer)

- better cross node mmap function

- MIMALLOC_RESERVE_HUGE_OS_PAGES mi_option_reserve_os_memory

- https://github.com/microsoft/mimalloc/issues/532

- safely destroy head and remove the ref in the simple_parallel::heaps

# warning!

https://stackoverflow.com/questions/61009618/openmp-threads-not-activating-when-run-with-mpirun

can not free anything allocated before parallel section

you can not use gcc nested function in a multithread environment. see https://thephd.dev/lambdas-nested-functions-block-expressions-oh-my#now-what

glibc 2.8+ is required https://www.man7.org/linux/man-pages/man3/swapcontext.3.html#NOTES however, glibc 2.8 is released on 2008-04 so any modern system should fulfill this requirement.
https://elixir.bootlin.com/glibc/glibc-2.8/source/sysdeps/unix/sysv/linux/x86_64/makecontext.c#L83

some mmap flags is ignored. this might have negative impact on performance.

user should't initialize the MPI environment. the library will do it.

can not reuse any thread created (and malloced) before init.

user have to use the built-in mimalloc as their malloc library

https://discourse.cmake.org/t/object-library-dependencies-dont-seem-to-propagate-through-target-link-libraries/10419

only compatible with openmpi (hang with other mpi implementation)