#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

namespace mafia {

// Механизм, которым исполняются ходы игроков.
enum class EngineKind {
    Sequential,  // все игроки по очереди в одном потоке (удобно для отладки)
    Thread,      // каждый игрок в своем std::thread (базовый вариант по заданию)
    Coroutine,   // каждый игрок - корутина (доп. пункт 4)
};

// Параметры запуска игры (заполняются из командной строки).
struct Config {
    int players = 10;                 // N - количество игроков (> 4)
    int mafiaDivisor = 3;             // k - мафии будет max(1, N / k), k >= 3
    bool interactive = false;         // пользователь участвует как один из игроков
    bool openAnnouncements = false;   // открытые объявления (полная роль) или закрытые (только сторона)
    bool fullLog = false;             // выводить полный лог (все ходы, голоса, проверки)
    std::uint32_t seed = 0;           // зерно генератора случайных чисел
    bool seedGiven = false;           // seed задан явно (иначе берется случайный)
    EngineKind engine = EngineKind::Thread;
    std::string rolesConfig;          // путь к YAML-файлу с набором ролей (пусто - роли по умолчанию)
    std::string logDir = "logs";      // директория для файловых логов
    bool fileLog = true;              // писать ли логи в файлы
    int aiPlayers = 0;                // сколько игроков управляется AI-агентом

    bool showHelp = false;            // --help: вывести справку и выйти
    bool printConfig = false;         // --print-config: вывести разобранные параметры и выйти
    bool dumpRoles = false;           // --dump-roles: вывести раздачу ролей и выйти
};

// Ошибка разбора аргументов (неизвестный флаг, нет значения и т.п.).
class ConfigError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Разбирает аргументы командной строки. Бросает ConfigError при ошибке.
// Если seed не задан, выбирается случайный (и сохраняется в Config, чтобы игру можно было повторить).
Config parseArgs(int argc, const char* const argv[]);

// Текст справки (--help).
std::string usage(const std::string& program);

// Человекочитаемое описание конфигурации (--print-config).
std::string describe(const Config& config);

std::string engineName(EngineKind kind);

}  // namespace mafia
