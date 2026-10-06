#include <game/config.hpp>

#include <charconv>
#include <random>
#include <sstream>
#include <string_view>

namespace mafia {

namespace {

// Достает значение флага (следующий аргумент); если его нет - ошибка.
std::string_view takeValue(int argc, const char* const argv[], int& i, std::string_view flag) {
    if (i + 1 >= argc) throw ConfigError("для параметра " + std::string(flag) + " не указано значение");
    return argv[++i];
}

// Строго разбирает целое число: вся строка должна быть числом.
long long toInt(std::string_view text, std::string_view flag) {
    long long value = 0;
    auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec != std::errc{} || end != text.data() + text.size()) {
        throw ConfigError("параметр " + std::string(flag) + " ожидает целое число, получено \"" +
                          std::string(text) + "\"");
    }
    return value;
}

EngineKind toEngine(std::string_view text) {
    if (text == "seq" || text == "sequential") return EngineKind::Sequential;
    if (text == "thread" || text == "threads") return EngineKind::Thread;
    if (text == "coro" || text == "coroutine") return EngineKind::Coroutine;
    throw ConfigError("неизвестный движок \"" + std::string(text) + "\" (допустимо: thread, coro, seq)");
}

}  // namespace

Config parseArgs(int argc, const char* const argv[]) {
    Config config;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            config.showHelp = true;
        } else if (arg == "--players" || arg == "-n") {
            config.players = static_cast<int>(toInt(takeValue(argc, argv, i, arg), arg));
        } else if (arg == "--mafia-divisor" || arg == "-k") {
            config.mafiaDivisor = static_cast<int>(toInt(takeValue(argc, argv, i, arg), arg));
        } else if (arg == "--interactive") {
            config.interactive = true;
        } else if (arg == "--open-announcements") {
            config.openAnnouncements = true;
        } else if (arg == "--closed-announcements") {
            config.openAnnouncements = false;
        } else if (arg == "--full-log") {
            config.fullLog = true;
        } else if (arg == "--seed") {
            config.seed = static_cast<std::uint32_t>(toInt(takeValue(argc, argv, i, arg), arg));
            config.seedGiven = true;
        } else if (arg == "--engine") {
            config.engine = toEngine(takeValue(argc, argv, i, arg));
        } else if (arg == "--config") {
            config.rolesConfig = std::string(takeValue(argc, argv, i, arg));
        } else if (arg == "--log-dir") {
            config.logDir = std::string(takeValue(argc, argv, i, arg));
        } else if (arg == "--no-file-log") {
            config.fileLog = false;
        } else if (arg == "--ai") {
            config.aiPlayers = static_cast<int>(toInt(takeValue(argc, argv, i, arg), arg));
        } else if (arg == "--print-config") {
            config.printConfig = true;
        } else if (arg == "--dump-roles") {
            config.dumpRoles = true;
        } else {
            throw ConfigError("неизвестный параметр \"" + std::string(arg) + "\" (см. --help)");
        }
    }

    if (config.showHelp) return config;
    if (config.players < 5) throw ConfigError("игроков должно быть больше 4 (--players N, N >= 5)");
    if (config.mafiaDivisor < 3) throw ConfigError("делитель мафии k должен быть >= 3 (--mafia-divisor k)");
    if (config.aiPlayers < 0 || config.aiPlayers > config.players) {
        throw ConfigError("--ai должно быть от 0 до числа игроков");
    }
    if (!config.seedGiven) config.seed = std::random_device{}();
    return config;
}

std::string usage(const std::string& program) {
    std::ostringstream out;
    out << "Эмуляция игры \"Мафия\"\n\n"
        << "Использование: " << program << " [параметры]\n\n"
        << "Основные параметры:\n"
        << "  --players N, -n N         количество игроков, N > 4 (по умолчанию 10)\n"
        << "  --interactive             вы участвуете как один из игроков\n"
        << "                            (иначе все игроки моделируются программно)\n"
        << "  --open-announcements      открытые объявления: ведущий называет роль выбывшего,\n"
        << "                            кого убили и кого спас Доктор\n"
        << "  --closed-announcements    закрытые объявления: только мирный / мафия (по умолчанию)\n"
        << "  --full-log                полный лог: все ночные ходы, голоса, проверки\n\n"
        << "Дополнительно:\n"
        << "  --mafia-divisor k, -k k   мафии будет max(1, N / k), k >= 3 (по умолчанию 3)\n"
        << "  --seed S                  зерно случайности (одинаковый seed - одинаковая игра)\n"
        << "  --engine thread|coro|seq  исполнение ходов: потоки (по умолчанию), корутины,\n"
        << "                            последовательно\n"
        << "  --config FILE             YAML-файл с набором ролей\n"
        << "  --log-dir DIR             директория для логов (по умолчанию logs)\n"
        << "  --no-file-log             не писать логи в файлы\n"
        << "  --ai M                    M игроков управляются AI-агентом\n"
        << "  --print-config            показать разобранные параметры и выйти\n"
        << "  --dump-roles              показать раздачу ролей и выйти\n"
        << "  --help, -h                эта справка\n\n"
        << "Пример: " << program << " --players 10 --interactive --open-announcements --full-log\n";
    return out.str();
}

std::string engineName(EngineKind kind) {
    switch (kind) {
        case EngineKind::Sequential: return "seq";
        case EngineKind::Thread: return "thread";
        case EngineKind::Coroutine: return "coro";
    }
    return "?";
}

std::string describe(const Config& config) {
    std::ostringstream out;
    out << "players:        " << config.players << "\n"
        << "mafia divisor:  " << config.mafiaDivisor << "\n"
        << "interactive:    " << (config.interactive ? "yes" : "no") << "\n"
        << "announcements:  " << (config.openAnnouncements ? "open" : "closed") << "\n"
        << "full log:       " << (config.fullLog ? "yes" : "no") << "\n"
        << "seed:           " << config.seed << (config.seedGiven ? "" : " (random)") << "\n"
        << "engine:         " << engineName(config.engine) << "\n"
        << "roles config:   " << (config.rolesConfig.empty() ? "(default)" : config.rolesConfig) << "\n"
        << "file log:       " << (config.fileLog ? config.logDir : "off") << "\n"
        << "ai players:     " << config.aiPlayers << "\n";
    return out.str();
}

}  // namespace mafia
