include(cmake/CPM.cmake)

# Done as a function so that updates to variables like
# CMAKE_CXX_FLAGS don't propagate out to other
# targets
function(simple_parallel_setup_dependencies)

    set(MPI_CXX_SKIP_MPICXX ON)
    find_package(MPI REQUIRED COMPONENTS C CXX)

    find_package(OpenMP REQUIRED)

    find_package(Boost REQUIRED COMPONENTS mpi container headers)
    find_package(Microsoft.GSL REQUIRED)
    find_package(cppcoro REQUIRED)
    find_package(concurrentqueue REQUIRED)

endfunction()
