#pragma once

#include <bigmpi.h>
#include <cstdint>
#include <mpi.h>

namespace simple_parallel::mpi_util {
    enum tag_enum : int32_t {
        init,
        finalize,
        send_stack,
        send_heap,
        run_function,
        common,
    };

    // // https://www.mcs.anl.gov/research/projects/mpi/sendmode.html
    // enum send_mode {
    //     // MPI_Send will not return until you can use the send buffer. It may
    //     or
    //     // may not block (it is allowed to buffer, either on the sender or
    //     // receiver side, or to wait for the matching receive).
    //     send,
    //     // May buffer; returns immediately and you can use the send buffer. A
    //     // late add-on to the MPI specification. Should be used only when
    //     // absolutely necessary.
    //     bsend,
    //     // will not return until matching receive posted
    //     ssend,
    //     // May be used ONLY if matching receive already posted. User
    //     responsible
    //     // for writing a correct program.
    //     rsend,
    //     // Nonblocking send. But not necessarily asynchronous. You can NOT
    //     reuse
    //     // the send buffer until either a successful, wait/test or you KNOW
    //     that
    //     // the message has been received (see MPI_Request_free). Note also
    //     that
    //     // while the I refers to immediate, there is no performance
    //     requirement
    //     // on MPI_Isend. An immediate send must return to the user without
    //     // requiring a matching receive at the destination. An implementation
    //     is
    //     // free to send the data to the destination before returning, as long
    //     as
    //     // the send call does not block waiting for a matching receive.
    //     // Different strategies of when to send the data offer different
    //     // performance advantages and disadvantages that will depend on the
    //     // application.
    //     isend,
    //     // buffered nonblocking
    //     ibsend,
    //     // Synchronous nonblocking. Note that a Wait/Test will complete only
    //     // when the matching receive is posted.
    //     issend,
    //     // As with MPI_Rsend, but nonblocking.
    //     irsend
    // };

    // template <send_mode mode>
    // static auto send_to_all_worker(void* buffer,
    //                                int32_t count,
    //                                MPI_Datatype datatype,
    //                                int dest,
    //                                int tag,
    //                                MPI_Comm comm = MPI_COMM_WORLD,
    //                                MPI_Request* request = nullptr) -> void {
    //     switch (mode) {
    //     case send:
    //         MPI_Send(buffer, count, datatype, dest, tag, comm);
    //         break;
    //     case bsend:
    //         MPI_Bsend(buffer, count, datatype, dest, tag, comm);
    //         break;
    //     case ssend:
    //         MPI_Ssend(buffer, count, datatype, dest, tag, comm);
    //         break;
    //     case rsend:
    //         MPI_Rsend(buffer, count, datatype, dest, tag, comm);
    //         break;
    //     case isend:
    //         MPI_Isend(buffer, count, datatype, dest, tag, comm, request);
    //         break;
    //     case ibsend:
    //         MPI_Ibsend(buffer, count, datatype, dest, tag, comm, request);
    //         break;
    //     case issend:
    //         MPI_Issend(buffer, count, datatype, dest, tag, comm, request);
    //         break;
    //     case irsend:
    //         MPI_Irsend(buffer, count, datatype, dest, tag, comm, request);
    //         break;
    //     default:
    //         throw std::runtime_error("Unknown send mode");
    //     }
    // }

    // template <send_mode mode>
    // static auto send_to_all_worker(void* buffer,
    //                                int64_t count,
    //                                MPI_Datatype datatype,
    //                                int dest,
    //                                int tag,
    //                                MPI_Comm comm = MPI_COMM_WORLD,
    //                                MPI_Request* request = nullptr) -> void {
    //     static_assert(
    //         mode != bsend && mode != ibsend,
    //         "bsend and ibsend use int64 as count is not implemented! see "
    //         "https://github.com/jeffhammond/BigMPI/pull/17");
    //     switch (mode) {
    //     case send:
    //         MPIX_Send_x(buffer, count, datatype, dest, tag, comm);
    //         break;
    //     case bsend:
    //         // MPIX_Bsend_x(buffer, count, datatype, dest, tag, comm);
    //         // https://github.com/jeffhammond/BigMPI/pull/17
    //         throw std::runtime_error(
    //             "bsend not implemented! see "
    //             "https://github.com/jeffhammond/BigMPI/pull/17");
    //         break;
    //     case ssend:
    //         MPIX_Ssend_x(buffer, count, datatype, dest, tag, comm);
    //         break;
    //     case rsend:
    //         MPIX_Rsend_x(buffer, count, datatype, dest, tag, comm);
    //         break;
    //     case isend:
    //         MPIX_Isend_x(buffer, count, datatype, dest, tag, comm, request);
    //         break;
    //     case ibsend:
    //         // MPIX_Ibsend_x(buffer, count, datatype, dest, tag, comm,
    //         request); throw std::runtime_error(
    //             "ibsend not implemented! see "
    //             "https://github.com/jeffhammond/BigMPI/pull/17");
    //         break;
    //     case issend:
    //         MPIX_Issend_x(buffer, count, datatype, dest, tag, comm, request);
    //         break;
    //     case irsend:
    //         MPIX_Irsend_x(buffer, count, datatype, dest, tag, comm, request);
    //         break;
    //     default:
    //         throw std::runtime_error("Unknown send mode");
    //     }
    // }

    // template <typename T>
    // concept mpi_allowed_count_type =
    //     std::integral<T> && (sizeof(T) == 8 || sizeof(T) == 4);

    // template <send_mode mode, mpi_allowed_count_type T>
    // static auto send_to_all_workers(void* buffer,
    //                                 T count,
    //                                 MPI_Datatype datatype,
    //                                 int tag,
    //                                 MPI_Comm comm = MPI_COMM_WORLD,
    //                                 MPI_Request* request = nullptr) -> void {
    //     int comm_size{};
    //     MPI_Comm_size(comm, &comm_size);
    //     for (int i = 1; i < comm_size; ++i) {
    //         send_to_all_workers<mode>(
    //             buffer, count, datatype, i, tag, comm, request);
    //     }
    // }

} // namespace simple_parallel::mpi_util
