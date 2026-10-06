#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace mafia {

// Номер игрока: 1..N (совпадает с номером в имени "Игрок N").
using PlayerId = int;
inline constexpr PlayerId kNoPlayer = -1;

// Роль игрока.
enum class Role {
    Civilian,   // мирный житель
    Mafia,      // мафиози
    Commissar,  // комиссар: ночью проверяет или стреляет
    Doctor,     // доктор: ночью лечит
    Maniac,     // маньяк: одиночка, ночью убивает
};

// Команда (лагерь), за которую играет роль; определяет условие победы.
enum class Team {
    Town,    // мирные (мирный житель, комиссар, доктор)
    Mafia,   // клан мафии
    Maniac,  // маньяк играет сам за себя
};

// Фаза игры.
enum class Phase {
    Setup,     // раздача ролей
    Day,       // дневное обсуждение и голосование
    Night,     // ночные действия
    Finished,  // игра окончена
};

// Команда роли.
Team teamOf(Role role);

// Какую команду видит комиссар при проверке. Маньяк по правилам виден как мирный.
Team seenByCommissar(Role role);

// Название роли по-русски ("Мирный житель", "Мафия", ...).
std::string roleName(Role role);

// Название команды по-русски ("мирные", "мафия", "маньяк").
std::string teamName(Team team);

// Есть ли у роли ночное действие.
bool hasNightAction(Role role);

// Роль по английскому ключу ("civilian", "mafia", "commissar", ...), используется в конфиге.
std::optional<Role> roleFromKey(std::string_view key);

}  // namespace mafia
