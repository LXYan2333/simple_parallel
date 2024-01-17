include(cmake/CPM.cmake)

# Done as a function so that updates to variables like
# CMAKE_CXX_FLAGS don't propagate out to other
# targets
function(simple_parallel_setup_dependencies)

    set(MPI_CXX_SKIP_MPICXX ON)
    find_package(MPI REQUIRED COMPONENTS C)
    message(STATUS "Run: ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG} ${MPIEXEC_MAX_NUMPROCS} ${MPIEXEC_PREFLAGS} EXECUTABLE ${MPIEXEC_POSTFLAGS} ARGS")

    find_package(OpenMP REQUIRED)

    CPMAddPackage("gh:TheLartians/PackageProject.cmake@1.11.1")

    # add Boost::mpi
    set(TRY_BOOST_VERSION "1.84.0")
    set(BOOST_NOT_HEADER_ONLY_COMPONENTS_THAT_YOU_NEED "mpi")
    # set(BOOST_HEADER_ONLY_COMPONENTS_THAT_YOU_NEED "")

    set(IS_BOOST_LOCAL OFF)
    if(${CPM_LOCAL_PACKAGES_ONLY})
        message(STATUS "Trying to find Boost...")
        find_package(Boost ${TRY_BOOST_VERSION} REQUIRED COMPONENTS
            ${BOOST_NOT_HEADER_ONLY_COMPONENTS_THAT_YOU_NEED})
        set(IS_BOOST_LOCAL ON)
    elseif(${CPM_USE_LOCAL_PACKAGES} OR NOT ${CPM_DOWNLOAD_ALL})
        message(STATUS "Trying to use local Boost...")
        find_package(Boost ${TRY_BOOST_VERSION} COMPONENTS ${BOOST_NOT_HEADER_ONLY_COMPONENTS_THAT_YOU_NEED})
        if(${BOOST_FOUND})
            set(IS_BOOST_LOCAL ON)
            message(DEBUG "boost include dir: ${Boost_INCLUDE_DIRS}")
        endif()
    endif()

    if(NOT (${BOOST_FOUND}) OR (NOT DEFINED BOOST_FOUND))
        message(STATUS "Trying to download Boost...")
        set(BOOST_INCLUDE_LIBRARIES "mpi")
        CPMAddPackage(
            NAME    Boost
            URL     "https://github.com/boostorg/boost/releases/download/boost-${TRY_BOOST_VERSION}/boost-${TRY_BOOST_VERSION}.tar.xz"
            OPTIONS "BOOST_ENABLE_MPI ON")
        set(IS_BOOST_LOCAL OFF)
    endif()


    CPMAddPackage(
        NAME mimalloc
        GITHUB_REPOSITORY "microsoft/mimalloc"
        VERSION 2.1.2)
    CPMAddPackage(
        NAME GSL
        GITHUB_REPOSITORY "Microsoft/GSL"
        VERSION 4.0.0
        OPTIONS "GSL_INSTALL ON")
    CPMAddPackage(
        NAME cppcoro
        GIT_TAG a3082f56ba135a659f7386b00ff797ba441207ba
        GITHUB_REPOSITORY "andreasbuhr/cppcoro"
        OPTIONS "BUILD_TESTING OFF")

endfunction()
