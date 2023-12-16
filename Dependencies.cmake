include(cmake/CPM.cmake)

# Done as a function so that updates to variables like
# CMAKE_CXX_FLAGS don't propagate out to other
# targets
function(simple_parallel_setup_dependencies)

    find_package(MPI REQUIRED)
    message(STATUS "Run: ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG} ${MPIEXEC_MAX_NUMPROCS} ${MPIEXEC_PREFLAGS} EXECUTABLE ${MPIEXEC_POSTFLAGS} ARGS")

    # For each dependency, see if it's
    # already been provided to us by a parent project

    if(simple_parallel_PACKAGING_MAINTAINER_MODE)
        find_package(mimalloc CONFIG REQUIRED)
        find_package(fmt CONFIG REQUIRED)
        find_package(Microsoft.GSL CONFIG REQUIRED)
    else()
        if(NOT TARGET mimalloc)
            CPMAddPackage(
                NAME mimalloc
                GITHUB_REPOSITORY "microsoft/mimalloc"
                VERSION 2.1.2)
        endif()

        if(NOT TARGET fmt::fmt)
            CPMAddPackage("gh:fmtlib/fmt#10.1.1")
        endif()

        if(NOT TARGET Microsoft.GSL::GSL)
            CPMAddPackage("gh:Microsoft/GSL#v4.0.0")
        endif()
    endif()
endfunction()
