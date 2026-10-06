#include <game/state.hpp>

#include <algorithm>
#include <thread>

#include "test.hpp"

using namespace mafia;

namespace {

int countRole(const std::vector<Role>& roles, Role role) {
    return static_cast<int>(std::ranges::count(roles, role));
}

Config makeConfig(int players, bool interactive = false) {
    Config c;
    c.players = players;
    c.interactive = interactive;
    return c;
}

}  // namespace

TEST(model_teams_and_commissar_view) {
    CHECK(teamOf(Role::Civilian) == Team::Town);
    CHECK(teamOf(Role::Commissar) == Team::Town);
    CHECK(teamOf(Role::Doctor) == Team::Town);
    CHECK(teamOf(Role::Mafia) == Team::Mafia);
    CHECK(teamOf(Role::Maniac) == Team::Maniac);
    CHECK(seenByCommissar(Role::Maniac) == Team::Town);
    CHECK(seenByCommissar(Role::Mafia) == Team::Mafia);
    CHECK(!hasNightAction(Role::Civilian));
    CHECK(hasNightAction(Role::Doctor));
    CHECK(roleFromKey("doctor") == Role::Doctor);
    CHECK(!roleFromKey("wizard"));
}

TEST(model_mafia_count_formula) {
    CHECK_EQ(mafiaCount(5, 3), 1);
    CHECK_EQ(mafiaCount(6, 3), 2);
    CHECK_EQ(mafiaCount(10, 3), 3);
    CHECK_EQ(mafiaCount(10, 4), 2);
    CHECK_EQ(mafiaCount(5, 5), 1);
    CHECK_EQ(mafiaCount(7, 10), 1);  // не менее одного
    for (int n = 5; n <= 30; ++n) {
        for (int k = 3; k <= 5; ++k) CHECK_EQ(mafiaCount(n, k), std::max(1, n / k));
    }
}

TEST(model_assign_roles_composition) {
    for (int n = 5; n <= 30; ++n) {
        for (int k = 3; k <= 5; ++k) {
            std::mt19937 rng(static_cast<unsigned>(n * 100 + k));
            auto roles = assignRoles(n, k, defaultSpecialRoles(), rng);
            CHECK_EQ(static_cast<int>(roles.size()), n);
            CHECK_EQ(countRole(roles, Role::Mafia), mafiaCount(n, k));
            CHECK_EQ(countRole(roles, Role::Commissar), 1);
            CHECK_EQ(countRole(roles, Role::Doctor), 1);
            CHECK_EQ(countRole(roles, Role::Maniac), 1);
            CHECK_EQ(countRole(roles, Role::Civilian), n - mafiaCount(n, k) - 3);
        }
    }
}

TEST(model_assign_roles_drops_extra_specials) {
    std::mt19937 rng(1);
    std::vector<Role> many(10, Role::Doctor);
    auto roles = assignRoles(5, 3, many, rng);
    CHECK_EQ(countRole(roles, Role::Mafia), 1);
    CHECK_EQ(countRole(roles, Role::Doctor), 4);
}

TEST(model_same_seed_same_roles) {
    std::mt19937 a(42), b(42);
    auto ra = assignRoles(12, 3, defaultSpecialRoles(), a);
    auto rb = assignRoles(12, 3, defaultSpecialRoles(), b);
    CHECK(ra == rb);
    bool anyDifferent = false;
    for (int i = 0; i < 20 && !anyDifferent; ++i) {
        std::mt19937 x(static_cast<unsigned>(100 + i));
        anyDifferent = assignRoles(12, 3, defaultSpecialRoles(), x) != ra;
    }
    CHECK(anyDifferent);
}

TEST(model_game_state_basics) {
    std::mt19937 rng(7);
    auto state = createGameState(makeConfig(8, true), defaultSpecialRoles(), rng);
    CHECK_EQ(state->playerCount(), 8);
    CHECK_EQ(state->name(3), "Игрок3");
    CHECK(state->player(1).human);
    CHECK(!state->player(2).human);
    CHECK_EQ(state->aliveIds().size(), 8u);

    state->kill(3, 1, "казнён");
    CHECK(!state->isAlive(3));
    CHECK_EQ(state->player(3).deathCause, "казнён");
    auto alive = state->aliveIds();
    CHECK_EQ(alive.size(), 7u);
    CHECK(std::ranges::find(alive, 3) == alive.end());

    bool threw = false;
    try {
        state->player(9);
    } catch (const std::out_of_range&) {
        threw = true;
    }
    CHECK(threw);
}

TEST(model_game_state_shared_between_threads) {
    std::mt19937 rng(7);
    auto state = createGameState(makeConfig(10), defaultSpecialRoles(), rng);
    std::vector<std::thread> readers;
    for (int t = 0; t < 4; ++t) {
        readers.emplace_back([state] {
            for (int i = 0; i < 2000; ++i) {
                auto alive = state->aliveIds();
                if (alive.size() > 10) std::abort();
            }
        });
    }
    for (int id = 1; id <= 5; ++id) state->kill(id, 1, "тест");
    for (auto& th : readers) th.join();
    CHECK_EQ(state->aliveIds().size(), 5u);
    CHECK_EQ(state.use_count(), 1);
}
