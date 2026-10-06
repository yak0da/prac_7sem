#pragma once

#include <iostream>
#include <string>

#include "config.hpp"
#include "types.hpp"
#include "view.hpp"

namespace mafia {

class FileLogger;

// Вывод игры: краткий (события) и полный (--full-log).
// Формулировки объявлений зависят от open/closed announcements.
// При наличии FileLogger строки дублируются в файлы раундов.
class Logger {
public:
    explicit Logger(const Config& config, std::ostream& out = std::cout);

    void setFileLogger(FileLogger* fileLogger) { fileLogger_ = fileLogger; }

    void info(const std::string& message);
    void detail(const std::string& message);  // только при --full-log
    void event(const GameEvent& event);

    bool fullLog() const { return fullLog_; }
    bool openAnnouncements() const { return openAnnouncements_; }

    // Публичное описание выбывшего: роль (open) или только сторона (closed).
    std::string announcePlayer(const std::string& name, Role role) const;

    // Причина смерти для публики: в closed ночные детали скрыты.
    std::string announceCause(const std::string& internalCause) const;

    static std::string nightKindName(NightKind kind);

private:
    bool fullLog_ = false;
    bool openAnnouncements_ = false;
    std::ostream& out_;
    FileLogger* fileLogger_ = nullptr;
};

}  // namespace mafia
