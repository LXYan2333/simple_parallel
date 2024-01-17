#pragma once

#include <boost/mpi.hpp>
#include <boost/mpi/status.hpp>
#include <coroutine>
#include <optional>
#include <utility>

namespace simple_parallel::async_util {
    template <typename T>
    struct client_coroutine_handler {
        struct promise_type;
        using handle_type = std::coroutine_handle<promise_type>;

        struct promise_type {
            std::optional<T> yielded_value = std::nullopt;

            auto get_return_object() -> client_coroutine_handler {
                return client_coroutine_handler{
                    handle_type::from_promise(*this)};
            }

            static auto initial_suspend() -> std::suspend_always { return {}; }

            static auto final_suspend() noexcept -> std::suspend_always {
                return {};
            }

            auto unhandled_exception() noexcept -> void {}

            auto return_void() noexcept -> void {}

            auto yield_value(std::optional<T> r) -> std::suspend_always {
                yielded_value = std::move(r);
                return {};
            }
        };

        auto next_task() -> std::optional<T> {
            if (handle.done()) {
                return std::nullopt;
            } else {
                handle.resume();
                return std::exchange(handle.promise().yielded_value,
                                     std::nullopt);
            }
        }

        handle_type handle;

        explicit client_coroutine_handler(handle_type _handle) noexcept
            : handle{_handle} {}

        client_coroutine_handler(const client_coroutine_handler&) = delete;

        client_coroutine_handler(client_coroutine_handler&& other) noexcept
            : handle{std::exchange(other.handle, nullptr)} {}

        auto operator=(const client_coroutine_handler&)
            -> client_coroutine_handler& = delete;

        auto operator=(client_coroutine_handler&& other) noexcept
            -> client_coroutine_handler& {
            std::swap(handle, other.handle);
            return *this;
        }

        ~client_coroutine_handler() {
            if (handle) [[likely]] {
                handle.destroy();
            }
        }
    };

    struct server_coroutine_handler {
        struct promise_type;
        using handle_type = std::coroutine_handle<promise_type>;

        struct promise_type {

            auto get_return_object() -> server_coroutine_handler {
                return server_coroutine_handler{
                    handle_type::from_promise(*this)};
            }

            static auto initial_suspend() -> std::suspend_always { return {}; }

            static auto final_suspend() noexcept -> std::suspend_always {
                return {};
            }

            auto unhandled_exception() noexcept -> void {}

            auto return_void() noexcept -> void {}
        };

        handle_type handle;

        explicit server_coroutine_handler(handle_type _handle) noexcept
            : handle{_handle} {}

        server_coroutine_handler(const server_coroutine_handler&) = delete;

        server_coroutine_handler(server_coroutine_handler&& other) noexcept
            : handle{std::exchange(other.handle, nullptr)} {}

        auto operator=(const server_coroutine_handler&)
            -> server_coroutine_handler& = delete;

        auto operator=(server_coroutine_handler&& other) noexcept
            -> server_coroutine_handler& {
            std::swap(handle, other.handle);
            return *this;
        }

        ~server_coroutine_handler() {
            if (handle) [[likely]] {
                handle.destroy();
            }
        }
    };
} // namespace simple_parallel::async_util
