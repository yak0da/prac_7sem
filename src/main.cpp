#include <game/config.hpp>
#include <game/state.hpp>

#include <iostream>
#include <random>

namespace {

void dumpRoles(const mafia::GameState& state) {
    for (const auto& p : state.snapshot()) {
        std::cout << "  " << p.name << (p.human ? " (вы)" : "") << ": " << mafia::roleName(p.role)
                  << " [" << mafia::teamName(mafia::teamOf(p.role)) << "]\n";
    }
}

}  // namespace

int main(int argc, char** argv) {
    mafia::Config config;
    try {
        config = mafia::parseArgs(argc, argv);
    } catch (const mafia::ConfigError& e) {
        std::cerr << "Ошибка: " << e.what() << "\n";
        return 2;
    }

    if (config.showHelp) {
        std::cout << mafia::usage(argv[0]);
        return 0;
    }
    if (config.printConfig) {
        std::cout << mafia::describe(config);
        return 0;
    }

    std::mt19937 rng(config.seed);
    auto state = mafia::createGameState(config, mafia::defaultSpecialRoles(), rng);

    if (config.dumpRoles) {
        std::cout << "Раздача ролей (seed " << config.seed << ", игроков " << config.players
                  << ", мафии " << mafia::mafiaCount(config.players, config.mafiaDivisor) << "):\n";
        dumpRoles(*state);
        return 0;
    }

    std::cout << "Игра пока не реализована (seed " << config.seed << ").\n";
    return 0;
}
