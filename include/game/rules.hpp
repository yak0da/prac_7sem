#pragma once

#include <optional>
#include <string>
#include <vector>

#include "state.hpp"
#include "types.hpp"

namespace mafia {

// Один голос на дневном голосовании.
struct Vote {
    PlayerId voter = kNoPlayer;
    PlayerId target = kNoPlayer;
};

// Ночные действия, собранные ведущим за одну ночь.
struct NightActions {
    PlayerId doctorHeal = kNoPlayer;           // кого лечит Доктор
    PlayerId previousDoctorHeal = kNoPlayer;   // кого лечил прошлой ночью (нельзя повторить)
    PlayerId mafiaTarget = kNoPlayer;          // жертва мафии
    PlayerId maniacTarget = kNoPlayer;         // жертва Маньяка
    PlayerId commissarTarget = kNoPlayer;      // цель Комиссара
    bool commissarShoots = false;              // true - стреляет, false - проверяет
};

// Смерть за ночь (результат, не изменение состояния).
struct DeathRecord {
    PlayerId victim = kNoPlayer;
    std::string cause;  // "убит мафией", "убит маньяком", "убит комиссаром"
};

// Результат разрешения ночи: кого спасли, кого убили, что узнал Комиссар.
struct NightResult {
    PlayerId healed = kNoPlayer;                      // кого реально защитил Доктор
    bool healRejected = false;                        // Доктор пытался лечить того же, что и вчера
    std::vector<DeathRecord> deaths;                  // погибшие за ночь
    std::vector<PlayerId> savedTargets;               // в кого стреляли, но Доктор спас
    PlayerId commissarChecked = kNoPlayer;            // кого проверил Комиссар
    std::optional<Team> commissarSeenTeam;            // что увидел (для Маньяка - мирные)
};

// Подсчет голосов. При ничьей (равное большинство) - никто не выбывает.
std::optional<PlayerId> countVotes(const std::vector<Vote>& votes);

// Разрешение ночи: лечение, убийства мафии/Маньяка/Комиссара, проверка Комиссара.
// Не меняет состояние - только возвращает результат. Работает по снимку игроков.
NightResult resolveNight(const std::vector<PlayerInfo>& players, const NightActions& actions);

// Условие победы по живым игрокам. Пусто, если игра продолжается.
std::optional<Team> checkWinner(const std::vector<PlayerInfo>& players);

// Сколько живых игроков каждой команды (через ranges).
struct TeamCounts {
    int town = 0;
    int mafia = 0;
    int maniac = 0;
    int alive = 0;
};

TeamCounts countAliveByTeam(const std::vector<PlayerInfo>& players);

}  // namespace mafia
