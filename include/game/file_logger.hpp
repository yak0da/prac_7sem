#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "config.hpp"
#include "state.hpp"
#include "types.hpp"

namespace mafia {

struct GameOutcome;  // объявлен в host.hpp

// Запись логов игры в файлы через std::filesystem.
// Каталог: <logDir>/game_<дата-время>/, раунды: round_01.txt, ..., итог: summary.txt.
class FileLogger {
public:
    FileLogger() = default;

    // Создаёт директорию логов. Если fileLog выключен — ничего не делает.
    void beginGame(const Config& config);

    void beginRound(int day);
    void line(const std::string& text);
    void endRound();

    void writeSummary(const GameOutcome& outcome, const std::vector<PlayerInfo>& players);

    bool enabled() const { return enabled_; }
    const std::filesystem::path& directory() const { return dir_; }
    int roundsWritten() const { return roundsWritten_; }

private:
    bool enabled_ = false;
    std::filesystem::path dir_;
    std::ofstream roundFile_;
    int currentDay_ = 0;
    int roundsWritten_ = 0;
};

}  // namespace mafia
