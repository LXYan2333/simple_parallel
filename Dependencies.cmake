include(cmake/CPM.cmake)

# Done as a function so that updates to variables like
# CMAKE_CXX_FLAGS don't propagate out to other
# targets
function(simple_parallel_setup_dependencies)

    find_package(MPI REQUIRED)
    message(STATUS "Run: ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG} ${MPIEXEC_MAX_NUMPROCS} ${MPIEXEC_PREFLAGS} EXECUTABLE ${MPIEXEC_POSTFLAGS} ARGS")

    CPMAddPackage("gh:TheLartians/PackageProject.cmake@1.11.1")

    # add Boost::mpi
    set(TRY_BOOST_VERSION "1.84.0")
    set(BOOST_NOT_HEADER_ONLY_COMPONENTS_THAT_YOU_NEED "mpi")
    # set(BOOST_HEADER_ONLY_COMPONENTS_THAT_YOU_NEED "")

    set(IS_BOOST_LOCAL OFF)
    if(simple_parallel_FORCE_FIND_PACKAGE)
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

    if(simple_parallel_FORCE_FIND_PACKAGE)
        find_package(mimalloc CONFIG REQUIRED)
        find_package(Microsoft.GSL CONFIG REQUIRED)
    else()
        CPMAddPackage(
            NAME mimalloc
            GITHUB_REPOSITORY "microsoft/mimalloc"
            VERSION 2.1.2)
        CPMAddPackage(
            NAME GSL
            GITHUB_REPOSITORY "Microsoft/GSL"
            VERSION 4.0.0
            OPTIONS "GSL_INSTALL ON")
    endif()

endfunction()
