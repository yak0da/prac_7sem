#include <game/types.hpp>

namespace mafia {

Team teamOf(Role role) {
    switch (role) {
        case Role::Mafia:
        case Role::Ninja: return Team::Mafia;
        case Role::Maniac: return Team::Maniac;
        case Role::Civilian:
        case Role::Commissar:
        case Role::Doctor:
        case Role::Bodyguard:
        case Role::Witness: return Team::Town;
    }
    return Team::Town;
}

Team seenByCommissar(Role role) {
    if (role == Role::Maniac || role == Role::Ninja) return Team::Town;
    return teamOf(role);
}

bool isMafiaRole(Role role) {
    return teamOf(role) == Team::Mafia;
}

std::string roleName(Role role) {
    switch (role) {
        case Role::Civilian: return "Мирный житель";
        case Role::Mafia: return "Мафия";
        case Role::Commissar: return "Комиссар";
        case Role::Doctor: return "Доктор";
        case Role::Maniac: return "Маньяк";
        case Role::Ninja: return "Ниндзя";
        case Role::Bodyguard: return "Телохранитель";
        case Role::Witness: return "Свидетель";
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
    if (key == "ninja") return Role::Ninja;
    if (key == "bodyguard") return Role::Bodyguard;
    if (key == "witness") return Role::Witness;
    return std::nullopt;
}

}  // namespace mafia
