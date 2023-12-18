include(cmake/CPM.cmake)

# Done as a function so that updates to variables like
# CMAKE_CXX_FLAGS don't propagate out to other
# targets
function(simple_parallel_setup_dependencies)

    find_package(MPI REQUIRED)
    message(STATUS "Run: ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG} ${MPIEXEC_MAX_NUMPROCS} ${MPIEXEC_PREFLAGS} EXECUTABLE ${MPIEXEC_POSTFLAGS} ARGS")

    # For each dependency, see if it's
    # already been provided to us by a parent project

    if(simple_parallel_FORCE_FIND_PACKAGE)
        find_package(mimalloc CONFIG REQUIRED)
        find_package(Microsoft.GSL CONFIG REQUIRED)
    else()
        find_package(mimalloc CONFIG)
        if(NOT mimalloc_FOUND)
            CPMAddPackage(
                NAME mimalloc
                GITHUB_REPOSITORY "microsoft/mimalloc"
                VERSION 2.1.2)
            install(TARGETS mimalloc
                    EXPORT  simple_parallelTargets
                    LIBRARY     DESTINATION ${CMAKE_INSTALL_LIBDIR}
                    ARCHIVE     DESTINATION ${CMAKE_INSTALL_LIBDIR}
                    RUNTIME     DESTINATION ${CMAKE_INSTALL_BINDIR}
                    INCLUDES    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
        endif()

        find_package(Microsoft.GSL CONFIG)
        if(NOT Microsoft.GSL_FOUND)
            CPMAddPackage(
                NAME GSL
                GITHUB_REPOSITORY "Microsoft/GSL"
                VERSION 4.0.0
                OPTIONS "GSL_INSTALL ON")
            install(TARGETS GSL
                    EXPORT  simple_parallelTargets
                    LIBRARY     DESTINATION ${CMAKE_INSTALL_LIBDIR}
                    ARCHIVE     DESTINATION ${CMAKE_INSTALL_LIBDIR}
                    RUNTIME     DESTINATION ${CMAKE_INSTALL_BINDIR}
                    INCLUDES    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
        endif()
    endif()
endfunction()
