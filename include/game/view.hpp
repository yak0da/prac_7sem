#pragma once

#include <map>
#include <string>
#include <vector>

#include "types.hpp"

namespace mafia {

// Публичная информация об игроке (без роли).
struct PublicPlayer {
    PlayerId id = kNoPlayer;
    std::string name;
    bool alive = true;
};

// Ограниченный взгляд игрока на игру: только то, что ему положено знать.
struct GameView {
    PlayerId self = kNoPlayer;
    Role myRole = Role::Civilian;
    int day = 0;
    Phase phase = Phase::Setup;
    std::vector<PublicPlayer> players;
    std::vector<PlayerId> allies;            // для мафии - живые сообщники
    std::map<PlayerId, Team> checks;         // для Комиссара - результаты проверок
    std::map<PlayerId, int> trust;           // своё доверие к другим (-100..100)
    PlayerId previousHeal = kNoPlayer;       // для Доктора
    PlayerId lastAccused = kNoPlayer;        // кого обвинил в обсуждении сегодня
    std::vector<std::string> history;        // публичные события для AI-промпта
};

// Высказывание днём: "Игрок X - мафия/мирный".
struct Statement {
    PlayerId speaker = kNoPlayer;
    PlayerId about = kNoPlayer;
    Team claim = Team::Town;
    std::string text;
};

// Ночное решение одного игрока.
enum class NightKind {
    Skip,     // нет хода (мирный)
    Heal,     // Доктор
    Kill,     // мафия / Ниндзя (предложение) или Маньяк
    Check,    // Комиссар проверяет
    Shoot,    // Комиссар стреляет
    Protect,  // Телохранитель защищает
    Watch,    // Свидетель следит
};

struct PlayerNightDecision {
    NightKind kind = NightKind::Skip;
    PlayerId target = kNoPlayer;
};

// События, которые ведущий рассылает игрокам.
enum class EventKind {
    DayStart,
    StatementMade,
    VoteTallied,
    Executed,
    NightResolved,
    PlayerDied,
    GameEnded,
};

struct GameEvent {
    EventKind kind = EventKind::DayStart;
    int day = 0;
    PlayerId actor = kNoPlayer;
    PlayerId target = kNoPlayer;
    Team team = Team::Town;
    Role role = Role::Civilian;
    std::string message;
    bool openInfo = false;  // при открытых объявлениях роль известна всем
};

}  // namespace mafia
