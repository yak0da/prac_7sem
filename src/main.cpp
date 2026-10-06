#include <game/config.hpp>
#include <game/engine.hpp>
#include <game/host.hpp>
#include <game/logger.hpp>
#include <game/state.hpp>
#include <game/yaml_lite.hpp>

#include <iostream>
#include <memory>
#include <random>

namespace {

void dumpRoles(const mafia::GameState& state) {
    for (const auto& p : state.snapshot()) {
        std::cout << "  " << p.name << (p.human ? " (вы)" : "") << ": " << mafia::roleName(p.role)
                  << " [" << mafia::teamName(mafia::teamOf(p.role)) << "]\n";
    }
}

std::unique_ptr<mafia::Engine> makeEngine(mafia::EngineKind kind) {
    switch (kind) {
        case mafia::EngineKind::Sequential:
            return std::make_unique<mafia::SequentialEngine>();
        case mafia::EngineKind::Thread:
            return std::make_unique<mafia::ThreadEngine>();
        case mafia::EngineKind::Coroutine:
            return std::make_unique<mafia::CoroEngine>();
    }
    return std::make_unique<mafia::SequentialEngine>();
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

    std::vector<mafia::Role> specials;
    try {
        specials = mafia::specialRolesFor(config);
    } catch (const mafia::YamlError& e) {
        std::cerr << "Ошибка YAML: " << e.what() << "\n";
        return 2;
    }

    if (config.dumpRoles) {
        std::mt19937 rng(config.seed);
        auto state = mafia::createGameState(config, specials, rng);
        std::cout << "Раздача ролей (seed " << config.seed << ", игроков " << config.players
                  << ", мафии " << mafia::mafiaCount(config.players, config.mafiaDivisor) << "):\n";
        dumpRoles(*state);
        return 0;
    }

    try {
        mafia::Logger logger(config);
        mafia::Host host(config, makeEngine(config.engine), logger);
        mafia::GameOutcome outcome = host.run();
        if (!outcome.finished) {
            std::cerr << "Игра не завершилась корректно.\n";
            return 1;
        }
    } catch (const mafia::YamlError& e) {
        std::cerr << "Ошибка YAML: " << e.what() << "\n";
        return 2;
    }
    return 0;
}
