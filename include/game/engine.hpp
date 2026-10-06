#pragma once

#include "player.hpp"
#include "view.hpp"

namespace mafia {

// Механизм исполнения ходов. Логика правил живёт в Host, здесь - только как вызвать игрока.
class Engine {
public:
    virtual ~Engine() = default;

    virtual Statement askSpeak(Player& player, const GameView& view) = 0;
    virtual PlayerId askVote(Player& player, const GameView& view) = 0;
    virtual PlayerNightDecision askAct(Player& player, const GameView& view) = 0;
};

// Последовательное исполнение в одном потоке (отладка и шаг 5).
class SequentialEngine final : public Engine {
public:
    Statement askSpeak(Player& player, const GameView& view) override;
    PlayerId askVote(Player& player, const GameView& view) override;
    PlayerNightDecision askAct(Player& player, const GameView& view) override;
};

}  // namespace mafia
