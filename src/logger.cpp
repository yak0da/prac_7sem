#include <game/logger.hpp>

#include <game/file_logger.hpp>

namespace mafia {

Logger::Logger(const Config& config, std::ostream& out)
    : fullLog_(config.fullLog), openAnnouncements_(config.openAnnouncements), out_(out) {}

void Logger::info(const std::string& message) {
    out_ << message << "\n";
    if (fileLogger_) fileLogger_->line(message);
}

void Logger::detail(const std::string& message) {
    if (!fullLog_) return;
    const std::string line = "  [лог] " + message;
    out_ << line << "\n";
    if (fileLogger_) fileLogger_->line(line);
}

void Logger::event(const GameEvent& event) {
    if (!event.message.empty()) info(event.message);
}

std::string Logger::announcePlayer(const std::string& name, Role role) const {
    if (openAnnouncements_) return name + " (" + roleName(role) + ")";
    const Team publicTeam = (teamOf(role) == Team::Mafia) ? Team::Mafia : Team::Town;
    return name + " [" + teamName(publicTeam) + "]";
}

std::string Logger::announceCause(const std::string& internalCause) const {
    if (openAnnouncements_) return internalCause;
    if (internalCause == "казнён") return "казнён";
    return "убит ночью";
}

std::string Logger::nightKindName(NightKind kind) {
    switch (kind) {
        case NightKind::Skip: return "пропуск";
        case NightKind::Heal: return "лечение";
        case NightKind::Kill: return "убийство";
        case NightKind::Check: return "проверка";
        case NightKind::Shoot: return "выстрел";
        case NightKind::Protect: return "защита";
        case NightKind::Watch: return "слежка";
    }
    return "?";
}

}  // namespace mafia
