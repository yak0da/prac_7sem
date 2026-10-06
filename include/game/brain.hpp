#pragma once

#include <random>

#include "view.hpp"

namespace mafia {

// "Мозг" игрока: принимает решения. Разные реализации - бот, человек, AI.
class Brain {
public:
    virtual ~Brain() = default;

    virtual Statement speak(const GameView& view, std::mt19937& rng) = 0;
    virtual PlayerId vote(const GameView& view, std::mt19937& rng) = 0;
    virtual PlayerNightDecision act(const GameView& view, std::mt19937& rng) = 0;
};

// Эвристический бот: опирается на доверие и знания роли.
class BotBrain : public Brain {
public:
    Statement speak(const GameView& view, std::mt19937& rng) override;
    PlayerId vote(const GameView& view, std::mt19937& rng) override;
    PlayerNightDecision act(const GameView& view, std::mt19937& rng) override;
};

}  // namespace mafia
