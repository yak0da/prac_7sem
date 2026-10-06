#include <game/file_logger.hpp>

#include <game/host.hpp>

#include <chrono>
#include <iomanip>
#include <sstream>

namespace mafia {

namespace {

std::string timestampTag() {
    using clock = std::chrono::system_clock;
    const auto now = clock::now();
    const std::time_t t = clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream out;
    out << std::put_time(&tm, "%Y%m%d_%H%M%S");
    return out.str();
}

}  // namespace

void FileLogger::beginGame(const Config& config) {
    enabled_ = config.fileLog;
    roundsWritten_ = 0;
    currentDay_ = 0;
    if (!enabled_) return;

    dir_ = std::filesystem::path(config.logDir) / ("game_" + timestampTag());
    std::filesystem::create_directories(dir_);
}

void FileLogger::beginRound(int day) {
    if (!enabled_) return;
    endRound();
    currentDay_ = day;
    std::ostringstream name;
    name << "round_" << std::setw(2) << std::setfill('0') << day << ".txt";
    roundFile_.open(dir_ / name.str(), std::ios::out | std::ios::trunc);
    if (roundFile_) {
        roundFile_ << "=== Раунд / день " << day << " ===\n";
    }
}

void FileLogger::line(const std::string& text) {
    if (!enabled_ || !roundFile_) return;
    roundFile_ << text << '\n';
}

void FileLogger::endRound() {
    if (!enabled_) return;
    if (roundFile_.is_open()) {
        roundFile_.close();
        ++roundsWritten_;
    }
    currentDay_ = 0;
}

void FileLogger::writeSummary(const GameOutcome& outcome, const std::vector<PlayerInfo>& players) {
    if (!enabled_) return;
    endRound();

    std::ofstream summary(dir_ / "summary.txt");
    if (!summary) return;

    summary << "=== Итоги игры ===\n";
    summary << "Победитель: " << teamName(outcome.winner) << "\n";
    summary << "Дней сыграно: " << outcome.daysPlayed << "\n";
    summary << "Завершена: " << (outcome.finished ? "да" : "нет") << "\n\n";
    summary << "Статистика по игрокам:\n";
    for (const auto& p : players) {
        summary << "- " << p.name << ": роль=" << roleName(p.role)
                << ", команда=" << teamName(teamOf(p.role))
                << ", статус=" << (p.alive ? "жив" : "выбыл");
        if (!p.alive) {
            summary << " (день " << p.diedOnDay << ", " << p.deathCause << ")";
        }
        if (p.human) summary << " [человек]";
        summary << "\n";
    }
}

}  // namespace mafia
