#include <game/logger.hpp>

#include <game/types.hpp>

namespace mafia {

Logger::Logger(const Config& config, std::ostream& out) : fullLog_(config.fullLog), out_(out) {}

void Logger::info(const std::string& message) { out_ << message << "\n"; }

void Logger::detail(const std::string& message) {
    if (fullLog_) out_ << "  [лог] " << message << "\n";
}

void Logger::event(const GameEvent& event) {
    if (!event.message.empty()) info(event.message);
}

}  // namespace mafia
