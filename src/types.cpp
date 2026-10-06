#include <game/types.hpp>

namespace mafia {

Team teamOf(Role role) {
    switch (role) {
        case Role::Mafia: return Team::Mafia;
        case Role::Maniac: return Team::Maniac;
        case Role::Civilian:
        case Role::Commissar:
        case Role::Doctor: return Team::Town;
    }
    return Team::Town;
}

Team seenByCommissar(Role role) {
    return role == Role::Maniac ? Team::Town : teamOf(role);
}

std::string roleName(Role role) {
    switch (role) {
        case Role::Civilian: return "Мирный житель";
        case Role::Mafia: return "Мафия";
        case Role::Commissar: return "Комиссар";
        case Role::Doctor: return "Доктор";
        case Role::Maniac: return "Маньяк";
    }
    return "?";
}

std::string teamName(Team team) {
    switch (team) {
        case Team::Town: return "мирные";
        case Team::Mafia: return "мафия";
        case Team::Maniac: return "маньяк";
    }
    return "?";
}

bool hasNightAction(Role role) {
    return role != Role::Civilian;
}

std::optional<Role> roleFromKey(std::string_view key) {
    if (key == "civilian") return Role::Civilian;
    if (key == "mafia") return Role::Mafia;
    if (key == "commissar") return Role::Commissar;
    if (key == "doctor") return Role::Doctor;
    if (key == "maniac") return Role::Maniac;
    return std::nullopt;
}

}  // namespace mafia
