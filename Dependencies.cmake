include(cmake/CPM.cmake)

# Done as a function so that updates to variables like
# CMAKE_CXX_FLAGS don't propagate out to other
# targets
function(simple_parallel_setup_dependencies)

    find_package(MPI REQUIRED)
    message(STATUS "Run: ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG} ${MPIEXEC_MAX_NUMPROCS} ${MPIEXEC_PREFLAGS} EXECUTABLE ${MPIEXEC_POSTFLAGS} ARGS")

    # find_package(hwloc REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(HWLOC REQUIRED IMPORTED_TARGET hwloc)


    # For each dependency, see if it's
    # already been provided to us by a parent project

    if(NOT TARGET mimalloc)
        CPMAddPackage(
            NAME mimalloc
            GITHUB_REPOSITORY "microsoft/mimalloc"
            VERSION 2.1.2
            OPTIONS "MI_OVERRIDE OFF")
    endif()

    if(NOT TARGET CLI11::CLI11)
        CPMAddPackage("gh:CLIUtils/CLI11@2.3.2")
    endif()

    if(NOT TARGET ittnotify)
        CPMAddPackage(
            NAME
            ittnotify
            VERSION
            3.24.0
            GITHUB_REPOSITORY
            "intel/ittapi")
    endif()

    if(NOT TARGET GTest)
        CPMAddPackage(
        NAME googletest
        VERSION 1.14.0
        GITHUB_REPOSITORY "google/googletest"
        OPTIONS "gtest_force_shared_crt ON")
    endif()

    if(NOT TARGET TBB)
        # set(BUILD_SHARED_LIBS ON)
        CPMAddPackage(
            NAME TBB
            VERSION 2021.10.0
            GITHUB_REPOSITORY "oneapi-src/oneTBB"
            OPTIONS
            "TBB_TEST OFF"
            "TBBMALLOC_BUILD OFF"
            "TBBMALLOC_PROXY_BUILD OFF"
            "BUILD_SHARED_LIBS ON")
    endif()

    if(NOT TARGET cppcoro)
        CPMAddPackage(
            NAME cppcoro
            GIT_TAG a3082f56ba135a659f7386b00ff797ba441207ba
            GITHUB_REPOSITORY "andreasbuhr/cppcoro"
            OPTIONS "BUILD_TESTING OFF")
    endif()
  
    if(NOT TARGET range-v3)
        CPMAddPackage(
            NAME range-v3
            GIT_TAG 0.12.0
            GITHUB_REPOSITORY "ericniebler/range-v3")
    endif()

    if(NOT TARGET fmtlib::fmtlib)
        CPMAddPackage("gh:fmtlib/fmt#10.1.1")
    endif()

    if(NOT TARGET Microsoft.GSL::GSL)
        CPMAddPackage("gh:Microsoft/GSL#v4.0.0")
    endif()

endfunction()
