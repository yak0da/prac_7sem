#pragma once

#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "view.hpp"

namespace mafia {

// "Мозг" игрока: принимает решения. Разные реализации - бот, человек.
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

// Человек за консолью: ведущий печатает подсказки, игрок вводит номера целей.
// При ошибке ввода или конце потока (EOF) — запасной ход BotBrain, чтобы тесты с пайпом не зависали.
class HumanBrain : public Brain {
public:
    explicit HumanBrain(std::istream& in = std::cin, std::ostream& out = std::cout);

    Statement speak(const GameView& view, std::mt19937& rng) override;
    PlayerId vote(const GameView& view, std::mt19937& rng) override;
    PlayerNightDecision act(const GameView& view, std::mt19937& rng) override;

private:
    void printHeader(const GameView& view, const std::string& phaseTitle) const;
    void printAlive(const GameView& view, bool includeSelf) const;
    // Читает номер игрока из списка допустимых; при неудаче — nullopt.
    std::optional<PlayerId> readPlayerId(const std::vector<PlayerId>& allowed) const;
    std::optional<int> readInt() const;

    std::istream* in_;
    std::ostream* out_;
    BotBrain fallback_;
};

}  // namespace mafia
