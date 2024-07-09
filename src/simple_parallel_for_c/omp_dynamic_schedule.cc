#include <array>
#include <boost/mpi/communicator.hpp>
#include <cassert>
#include <cppcoro/generator.hpp>
#include <cstddef>
#include <mpi.h>
#include <simple_parallel/dynamic_schedule.h>

#include <simple_parallel_for_c/omp_dynamic_schedule.h>
#include <utility>

using task_buffer_t = std::array<char, 16>;
using generator_t   = cppcoro::generator<task_buffer_t>;

struct dynamic_schedule_context {
    simple_parallel::detail::dynamic_schedule<generator_t> scheduler;
};

struct generator_context {
    generator_t                                   generator;
    decltype(std::declval<generator_t>().begin()) begin;
    decltype(std::declval<generator_t>().end())   end;
};

namespace {

    auto get_generator(bool (*scheduler_func)(void* state, void* task_buffer),
                       void* state) -> generator_t {
        task_buffer_t task_buffer;
        while (scheduler_func(state, task_buffer.data())) {
            co_yield task_buffer;
        }
    }

} // namespace

extern "C" {


    size_t dynamic_schedule_context_size() {
        return sizeof(struct dynamic_schedule_context);
    }

    void construct_dynamic_schedule_context(
        dynamic_schedule_context* dynamic_schedule_context_buffer_ptr,
        bool (*scheduler_func)(void* state, void* task_buffer),
        void*    state,
        size_t   prefetch_count,
        MPI_Comm comm) {
        bmpi::communicator bmpi_comm{comm, bmpi::comm_duplicate};
        simple_parallel::detail::dynamic_schedule<generator_t> scheduler{
            {bmpi_comm, bmpi::comm_attach},
            get_generator(scheduler_func, state),
            prefetch_count
        };


        new (dynamic_schedule_context_buffer_ptr)(
            struct dynamic_schedule_context)(std::move(scheduler));
    }

    void begin_schedule(
        dynamic_schedule_context* dynamic_schedule_context_buffer_ptr) {
        dynamic_schedule_context_buffer_ptr->scheduler.schedule();
    }

    void destruct_dynamic_schedule_context(
        dynamic_schedule_context* dynamic_schedule_context_buffer_ptr) {
        dynamic_schedule_context_buffer_ptr->~dynamic_schedule_context();
    }

    size_t thread_generator_context_buffer_size() {
        return sizeof(generator_context);
    }

    void construct_thread_task_generator(
        generator_context*        generator_context_buffer_ptr,
        dynamic_schedule_context* dynamic_schedule_context_buffer_ptr,
        void**                    buffer_ptr) {
        auto generator = dynamic_schedule_context_buffer_ptr->scheduler.gen(
            dynamic_schedule_context_buffer_ptr->scheduler);
        auto begin = generator.begin();
        auto end   = generator.end();
        new (generator_context_buffer_ptr)(generator_context)(
            std::move(generator), std::move(begin), std::move(end));
        *buffer_ptr = generator_context_buffer_ptr->begin->data();
    }

    void thread_generator_next(generator_context* generator_context_buffer_ptr,
                               void**             buffer_ptr) {
        ++generator_context_buffer_ptr->begin;
        *buffer_ptr = generator_context_buffer_ptr->begin->data();
    }

    bool thread_generator_end(generator_context* generator_context_buffer_ptr) {
        return generator_context_buffer_ptr->begin
               == generator_context_buffer_ptr->end;
    }

    void destruct_thread_task_generator(
        generator_context* generator_context_buffer_ptr) {
        generator_context_buffer_ptr->~generator_context();
    }
}
