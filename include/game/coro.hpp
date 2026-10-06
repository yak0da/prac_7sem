#pragma once

#include <coroutine>
#include <exception>
#include <queue>
#include <utility>

namespace mafia {

// Простой однопоточный планировщик готовых корутин.
class Scheduler {
public:
    void schedule(std::coroutine_handle<> handle) {
        if (handle && !handle.done()) ready_.push(handle);
    }

    bool empty() const { return ready_.empty(); }

    // Возобновить одну готовую корутину. false, если очередь пуста.
    bool runOne() {
        if (ready_.empty()) return false;
        auto handle = ready_.front();
        ready_.pop();
        handle.resume();
        return true;
    }

    // Крутить очередь, пока predicate() ложен (или очередь не опустеет без прогресса).
    template <class Pred>
    void runUntil(Pred&& predicate) {
        while (!predicate()) {
            if (!runOne()) break;
        }
    }

private:
    std::queue<std::coroutine_handle<>> ready_;
};

// Корутина-задача без результата (fire-and-forget / долгий цикл игрока).
class Task {
public:
    struct promise_type {
        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() noexcept {}
        void unhandled_exception() { exception_ = std::current_exception(); }

        std::exception_ptr exception_;
    };

    Task() = default;
    explicit Task(std::coroutine_handle<promise_type> handle) : handle_(handle) {}

    Task(Task&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            destroy();
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
    }

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    ~Task() { destroy(); }

    std::coroutine_handle<promise_type> handle() const { return handle_; }

    void rethrowIfNeeded() const {
        if (handle_ && handle_.promise().exception_) {
            std::rethrow_exception(handle_.promise().exception_);
        }
    }

private:
    void destroy() {
        if (handle_) {
            handle_.destroy();
            handle_ = {};
        }
    }

    std::coroutine_handle<promise_type> handle_{};
};

}  // namespace mafia
