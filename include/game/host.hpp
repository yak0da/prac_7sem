#pragma once

#include <smart/shared_ptr.hpp>

#include <memory>
#include <optional>
#include <vector>

#include "config.hpp"
#include "engine.hpp"
#include "logger.hpp"
#include "player.hpp"
#include "state.hpp"

namespace mafia {

// Итог партии (для тестов и вывода).
struct GameOutcome {
    Team winner = Team::Town;
    int daysPlayed = 0;
    bool doctorTriedSameHealTwice = false;
    bool finished = false;
    smart::SharedPtr<GameState> state;
};

// Ведущий: точка синхронизации, цикл дня/ночи, объявления, проверка победы.
class Host {
public:
    Host(Config config, std::unique_ptr<Engine> engine, Logger logger);

    GameOutcome run();

private:
    GameView buildView(const Player& player) const;
    void broadcast(const GameEvent& event);
    void applyDeath(PlayerId id, int day, const std::string& cause, Role role, bool announceRole);
    std::optional<Team> maybeFinish(GameOutcome& outcome);

    Config config_;
    std::unique_ptr<Engine> engine_;
    Logger logger_;
    smart::SharedPtr<GameState> state_;
    std::vector<smart::SharedPtr<Player>> players_;
    PlayerId previousDoctorHeal_ = kNoPlayer;
    static constexpr int kMaxDays = 100;
};

}  // namespace mafia
