#pragma once

#include <boost/mpi.hpp>

namespace simple_parallel {
void sync_fortran_global_variables(const boost::mpi::communicator &comm,
                                   int root_rank);
}