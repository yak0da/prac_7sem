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
    Ninja,      // мафиози; при проверке Комиссара виден мирным
    Bodyguard,  // мирный; защищает и убивает нападавшего (от Кома — только спасает)
    Witness,    // мирный; следит и при покушении узнаёт убийцу
};

// Команда (лагерь), за которую играет роль; определяет условие победы.
enum class Team {
    Town,    // мирные
    Mafia,   // клан мафии (включая Ниндзя)
    Maniac,  // маньяк играет сам за себя
};

// Фаза игры.
enum class Phase {
    Setup,     // раздача ролей
    Day,       // дневное обсуждение и голосование
    Night,     // ночные действия
    Finished,  // игра окончена
};

Team teamOf(Role role);

// Какую команду видит комиссар при проверке.
// Маньяк и Ниндзя видны как мирные.
Team seenByCommissar(Role role);

// Относится ли роль к мафиозному клану (обычная мафия или Ниндзя).
bool isMafiaRole(Role role);

std::string roleName(Role role);
std::string teamName(Team team);
bool hasNightAction(Role role);
std::optional<Role> roleFromKey(std::string_view key);

}  // namespace mafia
