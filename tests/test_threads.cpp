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
    if (engine == EngineKind::Thread) eng = std::make_unique<ThreadEngine>();
    else eng = std::make_unique<SequentialEngine>();

    Host host(config, std::move(eng), logger);
    return host.run();
}

}  // namespace

TEST(threads_matches_sequential_single_seed) {
    auto seq = playWith(EngineKind::Sequential, 10, 42);
    auto thr = playWith(EngineKind::Thread, 10, 42);
    CHECK(seq.finished);
    CHECK(thr.finished);
    CHECK(seq.winner == thr.winner);
    CHECK_EQ(seq.daysPlayed, thr.daysPlayed);
    CHECK(seq.state->aliveIds() == thr.state->aliveIds());
    for (const auto& a : seq.state->snapshot()) {
        auto b = thr.state->player(a.id);
        CHECK(a.alive == b.alive);
        CHECK(a.role == b.role);
        CHECK_EQ(a.deathCause, b.deathCause);
    }
}

TEST(threads_matches_sequential_many_seeds) {
    int mismatches = 0;
    for (int n = 5; n <= 12; ++n) {
        for (int s = 0; s < 25; ++s) {
            const std::uint32_t seed = static_cast<std::uint32_t>(n * 100 + s);
            auto seq = playWith(EngineKind::Sequential, n, seed);
            auto thr = playWith(EngineKind::Thread, n, seed);
            if (!seq.finished || !thr.finished || seq.winner != thr.winner ||
                seq.daysPlayed != thr.daysPlayed || seq.state->aliveIds() != thr.state->aliveIds()) {
                ++mismatches;
            }
        }
    }
    CHECK_EQ(mismatches, 0);
}

TEST(threads_runs_without_hang) {
    for (std::uint32_t seed = 1; seed <= 20; ++seed) {
        auto o = playWith(EngineKind::Thread, 12, seed);
        CHECK(o.finished);
    }
}
