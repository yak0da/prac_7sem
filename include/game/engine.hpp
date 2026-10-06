#pragma once

#include <smart/shared_ptr.hpp>

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "coro.hpp"
#include "player.hpp"
#include "view.hpp"

namespace mafia {

// Механизм исполнения ходов. Логика правил живёт в Host, здесь - только как вызвать игрока.
class Engine {
public:
    virtual ~Engine() = default;

    // Запуск/остановка рабочих исполнителей (нити или корутины).
    virtual void start(const std::vector<smart::SharedPtr<Player>>& /*players*/) {}
    virtual void stop() {}

    virtual Statement askSpeak(Player& player, const GameView& view) = 0;
    virtual PlayerId askVote(Player& player, const GameView& view) = 0;
    virtual PlayerNightDecision askAct(Player& player, const GameView& view) = 0;
};

// Последовательное исполнение в одном потоке (отладка).
class SequentialEngine final : public Engine {
public:
    Statement askSpeak(Player& player, const GameView& view) override;
    PlayerId askVote(Player& player, const GameView& view) override;
    PlayerNightDecision askAct(Player& player, const GameView& view) override;
};

// Каждый игрок работает в своём потоке: ждёт запрос фазы, считает ход, отдаёт ведущему.
class ThreadEngine final : public Engine {
public:
    ThreadEngine() = default;
    ~ThreadEngine() override { stop(); }

    ThreadEngine(const ThreadEngine&) = delete;
    ThreadEngine& operator=(const ThreadEngine&) = delete;

    void start(const std::vector<smart::SharedPtr<Player>>& players) override;
    void stop() override;

    Statement askSpeak(Player& player, const GameView& view) override;
    PlayerId askVote(Player& player, const GameView& view) override;
    PlayerNightDecision askAct(Player& player, const GameView& view) override;

private:
    enum class RequestKind : std::uint8_t { None, Speak, Vote, Act };

    struct Slot {
        smart::SharedPtr<Player> player;
        std::mutex mutex;
        std::condition_variable cv;
        RequestKind request = RequestKind::None;
        bool responseReady = false;
        GameView view;
        Statement speakResult;
        PlayerId voteResult = kNoPlayer;
        PlayerNightDecision actResult;
    };

    void worker(Slot& slot, std::stop_token stopToken);
    Slot& slotFor(PlayerId id);

    template <class Result, class SetRequest, class GetResult>
    Result ask(Player& player, const GameView& view, SetRequest setRequest, GetResult getResult);

    std::unordered_map<PlayerId, smart::SharedPtr<Slot>> slots_;
    std::vector<std::jthread> threads_;
    bool running_ = false;
};

// Каждый игрок — корутина на однопоточном Scheduler.
// Ведущий по-прежнему вызывает ask* по одному → тот же порядок и тот же seed, что у seq/thread.
class CoroEngine final : public Engine {
public:
    CoroEngine() = default;
    ~CoroEngine() override { stop(); }

    CoroEngine(const CoroEngine&) = delete;
    CoroEngine& operator=(const CoroEngine&) = delete;

    void start(const std::vector<smart::SharedPtr<Player>>& players) override;
    void stop() override;

    Statement askSpeak(Player& player, const GameView& view) override;
    PlayerId askVote(Player& player, const GameView& view) override;
    PlayerNightDecision askAct(Player& player, const GameView& view) override;

private:
    enum class RequestKind : std::uint8_t { None, Speak, Vote, Act, Stop };

    struct Slot {
        smart::SharedPtr<Player> player;
        RequestKind request = RequestKind::None;
        bool responseReady = false;
        bool hasWaiting = false;
        std::coroutine_handle<> waiting;
        GameView view;
        Statement speakResult;
        PlayerId voteResult = kNoPlayer;
        PlayerNightDecision actResult;
        Task task;
    };

    // co_await: заснуть, пока ведущий не положит запрос в слот.
    struct WaitRequest {
        Slot* slot = nullptr;
        Scheduler* scheduler = nullptr;

        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<> handle) const {
            slot->waiting = handle;
            slot->hasWaiting = true;
        }
        RequestKind await_resume() const noexcept {
            return slot->request;
        }
    };

    static Task playerLoop(smart::SharedPtr<Slot> slot, Scheduler* scheduler);
    Slot& slotFor(PlayerId id);

    template <class Result, class GetResult>
    Result ask(Player& player, const GameView& view, RequestKind kind, GetResult getResult);

    Scheduler scheduler_;
    std::unordered_map<PlayerId, smart::SharedPtr<Slot>> slots_;
    bool running_ = false;
};

}  // namespace mafia
