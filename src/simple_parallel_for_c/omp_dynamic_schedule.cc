#include <simple_parallel_for_c/omp_dynamic_schedule.h>

#include <array>
#include <boost/mpi.hpp>
#include <cassert>
#include <cppcoro/generator.hpp>
#include <cstddef>
#include <mpi.h>
#include <simple_parallel/dynamic_schedule.h>
#include <utility>

using task_buffer_t = std::array<char, S_P_DEFAULT_C_TASK_BUFFER_SIZE>;
using generator_t   = cppcoro::generator<task_buffer_t>;
using scheduler_t   = simple_parallel::detail::dynamic_schedule<generator_t>;

struct dynamic_schedule_context {
    scheduler_t scheduler;
};

struct generator_context {
    generator_t generator;

    decltype(std::declval<generator_t>().begin()) current;
    decltype(std::declval<generator_t>().end())   sentinel;
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


    auto dynamic_schedule_context_size() -> size_t {
        return sizeof(struct dynamic_schedule_context);
    }

    auto construct_dynamic_schedule_context(
        dynamic_schedule_context* dynamic_schedule_context_buffer_ptr,
        bool (*scheduler_func)(void* state, void* task_buffer),
        void*    state,
        int      num_threads,
        MPI_Comm comm) -> void {
        new (dynamic_schedule_context_buffer_ptr)(
            struct dynamic_schedule_context)({
            {comm, bmpi::comm_attach},
            get_generator(scheduler_func, state),
            num_threads
        });
    }

    auto destruct_dynamic_schedule_context(
        dynamic_schedule_context* dynamic_schedule_context_buffer_ptr) -> void {
        dynamic_schedule_context_buffer_ptr->~dynamic_schedule_context();
    }

    auto thread_generator_context_buffer_size() -> size_t {
        return sizeof(generator_context);
    }

    auto construct_thread_task_generator(
        generator_context*        generator_context_buffer_ptr,
        dynamic_schedule_context* dynamic_schedule_context_buffer_ptr,
        void**                    task_buffer_ptr) -> void {
        auto generator = dynamic_schedule_context_buffer_ptr->scheduler.gen();
        auto begin     = generator.begin();
        auto end       = generator.end();
        new (generator_context_buffer_ptr)(generator_context)(
            std::move(generator), std::move(begin), std::move(end));
        *task_buffer_ptr = generator_context_buffer_ptr->current->data();
    }

    auto thread_generator_next(generator_context* generator_context_buffer_ptr,
                               void**             task_buffer_ptr) -> void {
        ++generator_context_buffer_ptr->current;
        *task_buffer_ptr = generator_context_buffer_ptr->current->data();
    }

    auto thread_generator_end(generator_context* generator_context_buffer_ptr)
        -> bool {
        return generator_context_buffer_ptr->current
               == generator_context_buffer_ptr->sentinel;
    }

    auto destruct_thread_task_generator(
        generator_context* generator_context_buffer_ptr) -> void {
        generator_context_buffer_ptr->~generator_context();
    }

    auto guided_self_scheduler(void* state, void* task_buffer) -> bool {
        auto* s    = static_cast<struct gss_state*>(state);
        auto* task = static_cast<simple_task*>(task_buffer);

        size_t unused_num = s->end - s->begin;

        if (s->begin < s->end) {
            size_t next_schedule = (unused_num - 1) / s->process_count + 1;

            // next_schedule should not be less than grain_size
            next_schedule = std::max(next_schedule, s->grain_size);

            // next_schedule should not be greater than unused_num
            next_schedule = std::min(next_schedule, unused_num);

            task->begin = s->begin;
            task->end   = s->begin + next_schedule;

            s->begin += next_schedule;
            return true;
        }
        return false;
    };

    auto liner_scheduler(void* state, void* task_buffer) -> bool {
        auto* s    = static_cast<struct liner_scheduler_state*>(state);
        auto* task = static_cast<struct simple_task*>(task_buffer);

        if (s->begin < s->end) {
            size_t next_schedule = s->grain_size;

            // next_schedule should not be greater than unused_num
            next_schedule = std::min(next_schedule, s->end - s->begin);

            task->begin = s->begin;
            task->end   = s->begin + next_schedule;

            s->begin += next_schedule;
            return true;
        }
        return false;
    };
}
