#pragma once

#include <smart/shared_ptr.hpp>

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "player.hpp"
#include "view.hpp"

namespace mafia {

// Механизм исполнения ходов. Логика правил живёт в Host, здесь - только как вызвать игрока.
class Engine {
public:
    virtual ~Engine() = default;

    // Запуск/остановка рабочих нитей (для ThreadEngine; Sequential - пустые).
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
// Ведущий по-прежнему вызывает ask* по одному - порядок как у SequentialEngine,
// поэтому при одинаковом seed исход совпадает (у каждого игрока свой rng).
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

}  // namespace mafia
