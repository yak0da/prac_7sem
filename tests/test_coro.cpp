#include <game/config.hpp>
#include <game/engine.hpp>
#include <game/host.hpp>
#include <game/logger.hpp>

#include <memory>
#include <sstream>

#include "test.hpp"

using namespace mafia;

namespace {

GameOutcome playWith(EngineKind engine, int players, std::uint32_t seed) {
    Config config;
    config.players = players;
    config.seed = seed;
    config.seedGiven = true;
    config.engine = engine;
    config.fileLog = false;
    config.fullLog = false;

    std::ostringstream sink;
    Logger logger(config, sink);
    std::unique_ptr<Engine> eng;
    switch (engine) {
        case EngineKind::Thread: eng = std::make_unique<ThreadEngine>(); break;
        case EngineKind::Coroutine: eng = std::make_unique<CoroEngine>(); break;
        default: eng = std::make_unique<SequentialEngine>(); break;
    }
    Host host(config, std::move(eng), logger);
    return host.run();
}

bool sameOutcome(const GameOutcome& a, const GameOutcome& b) {
    return a.finished && b.finished && a.winner == b.winner && a.daysPlayed == b.daysPlayed &&
           a.state->aliveIds() == b.state->aliveIds();
}

}  // namespace

TEST(coro_matches_sequential_single_seed) {
    auto seq = playWith(EngineKind::Sequential, 10, 42);
    auto coro = playWith(EngineKind::Coroutine, 10, 42);
    CHECK(sameOutcome(seq, coro));
}

TEST(coro_matches_thread_and_seq_many_seeds) {
    int mismatches = 0;
    for (int n = 5; n <= 12; ++n) {
        for (int s = 0; s < 20; ++s) {
            const std::uint32_t seed = static_cast<std::uint32_t>(n * 200 + s);
            auto seq = playWith(EngineKind::Sequential, n, seed);
            auto thr = playWith(EngineKind::Thread, n, seed);
            auto coro = playWith(EngineKind::Coroutine, n, seed);
            if (!sameOutcome(seq, thr) || !sameOutcome(seq, coro)) ++mismatches;
        }
    }
    CHECK_EQ(mismatches, 0);
}

TEST(coro_runs_without_hang) {
    for (std::uint32_t seed = 1; seed <= 25; ++seed) {
        auto o = playWith(EngineKind::Coroutine, 12, seed);
        CHECK(o.finished);
    }
}
