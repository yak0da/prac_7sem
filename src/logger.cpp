#include <game/logger.hpp>

namespace mafia {

Logger::Logger(const Config& config, std::ostream& out)
    : fullLog_(config.fullLog), openAnnouncements_(config.openAnnouncements), out_(out) {}

void Logger::info(const std::string& message) { out_ << message << "\n"; }

void Logger::detail(const std::string& message) {
    if (fullLog_) out_ << "  [лог] " << message << "\n";
}

void Logger::event(const GameEvent& event) {
    if (!event.message.empty()) info(event.message);
}

std::string Logger::announcePlayer(const std::string& name, Role role) const {
    if (openAnnouncements_) return name + " (" + roleName(role) + ")";
    // Закрытые объявления: только мирные / мафия. Маньяк не раскрывается как особый статус.
    const Team publicTeam = (teamOf(role) == Team::Mafia) ? Team::Mafia : Team::Town;
    return name + " [" + teamName(publicTeam) + "]";
}

std::string Logger::announceCause(const std::string& internalCause) const {
    if (openAnnouncements_) return internalCause;
    if (internalCause == "казнён") return "казнён";
    // Обстоятельства ночного убийства и кем убит — в секрете.
    return "убит ночью";
}

std::string Logger::nightKindName(NightKind kind) {
    switch (kind) {
        case NightKind::Skip: return "пропуск";
        case NightKind::Heal: return "лечение";
        case NightKind::Kill: return "убийство";
        case NightKind::Check: return "проверка";
        case NightKind::Shoot: return "выстрел";
    }
    return "?";
}

}  // namespace mafia
