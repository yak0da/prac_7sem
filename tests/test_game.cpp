#include <game/config.hpp>
#include <game/engine.hpp>
#include <game/host.hpp>
#include <game/logger.hpp>
#include <game/rules.hpp>

#include <memory>
#include <sstream>

#include "test.hpp"

using namespace mafia;

namespace {

GameOutcome playSilent(int players, std::uint32_t seed) {
    Config config;
    config.players = players;
    config.seed = seed;
    config.seedGiven = true;
    config.engine = EngineKind::Sequential;
    config.fileLog = false;
    config.fullLog = false;

    std::ostringstream sink;
    Logger logger(config, sink);
    Host host(config, std::make_unique<SequentialEngine>(), logger);
    return host.run();
}

bool winnerConsistent(const GameOutcome& o) {
    if (!o.finished || !o.state) return false;
    auto expected = checkWinner(o.state->snapshot());
    // После конца игры победитель должен совпадать с checkWinner,
    // кроме редкого случая лимита дней — тогда checkWinner может быть пуст.
    if (expected) return *expected == o.winner;
    return true;
}

}  // namespace

TEST(game_single_seed_finishes) {
    auto o = playSilent(8, 1);
    CHECK(o.finished);
    CHECK(o.daysPlayed >= 1);
    CHECK(winnerConsistent(o));
}

TEST(game_same_seed_same_winner_and_days) {
    auto a = playSilent(10, 42);
    auto b = playSilent(10, 42);
    CHECK(a.finished);
    CHECK(b.finished);
    CHECK(a.winner == b.winner);
    CHECK_EQ(a.daysPlayed, b.daysPlayed);

    auto aliveA = a.state->aliveIds();
    auto aliveB = b.state->aliveIds();
    CHECK(aliveA == aliveB);
}

TEST(game_thousand_seeds) {
    int finished = 0;
    int doctorSame = 0;
    int inconsistent = 0;
    for (int n = 5; n <= 15; ++n) {
        for (int s = 0; s < 100; ++s) {
            const std::uint32_t seed = static_cast<std::uint32_t>(n * 1000 + s);
            auto o = playSilent(n, seed);
            if (o.finished) ++finished;
            if (o.doctorTriedSameHealTwice) ++doctorSame;
            if (!winnerConsistent(o)) ++inconsistent;

            // Мёртвые не ходят: у всех мёртвых alive=false согласован со state.
            for (const auto& info : o.state->snapshot()) {
                if (!info.alive) {
                    CHECK_EQ(info.diedOnDay > 0 || !info.deathCause.empty(), true);
                }
            }
        }
    }
    // 11 размеров * 100 seed = 1100 игр
    CHECK_EQ(finished, 1100);
    CHECK_EQ(doctorSame, 0);
    CHECK_EQ(inconsistent, 0);
}
