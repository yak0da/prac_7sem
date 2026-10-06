#pragma once

#include <iostream>
#include <string>

#include "config.hpp"
#include "view.hpp"

namespace mafia {

// Простой вывод в консоль. На шаге 6 будет расширен (открытые/закрытые объявления).
class Logger {
public:
    explicit Logger(const Config& config, std::ostream& out = std::cout);

    void info(const std::string& message);
    void detail(const std::string& message);  // только при --full-log
    void event(const GameEvent& event);

private:
    bool fullLog_ = false;
    std::ostream& out_;
};

}  // namespace mafia
