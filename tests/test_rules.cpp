#include <game/rules.hpp>

#include "test.hpp"

using namespace mafia;

namespace {

PlayerInfo makePlayer(PlayerId id, Role role, bool alive = true) {
    PlayerInfo p;
    p.id = id;
    p.name = "Игрок" + std::to_string(id);
    p.role = role;
    p.alive = alive;
    return p;
}

// Типовой состав: 1 мафия, комиссар, доктор, маньяк, мирные.
std::vector<PlayerInfo> samplePlayers() {
    return {
        makePlayer(1, Role::Mafia),
        makePlayer(2, Role::Commissar),
        makePlayer(3, Role::Doctor),
        makePlayer(4, Role::Maniac),
        makePlayer(5, Role::Civilian),
        makePlayer(6, Role::Civilian),
    };
}

}  // namespace

TEST(rules_vote_majority) {
    std::vector<Vote> votes{{1, 3}, {2, 3}, {4, 1}, {5, 3}};
    auto winner = countVotes(votes);
    CHECK(winner.has_value());
    CHECK_EQ(*winner, 3);
}

TEST(rules_vote_tie_nobody_leaves) {
    std::vector<Vote> votes{{1, 2}, {3, 2}, {4, 5}, {6, 5}};
    CHECK(!countVotes(votes).has_value());
}

TEST(rules_vote_empty) {
    CHECK(!countVotes({}).has_value());
}

TEST(rules_doctor_saves_from_mafia) {
    auto players = samplePlayers();
    NightActions actions;
    actions.doctorHeal = 5;
    actions.mafiaTarget = 5;
    NightResult r = resolveNight(players, actions);
    CHECK_EQ(r.healed, 5);
    CHECK(r.deaths.empty());
    CHECK_EQ(r.savedTargets.size(), 1u);
    CHECK_EQ(r.savedTargets.front(), 5);
}

TEST(rules_doctor_saves_from_commissar_shot) {
    auto players = samplePlayers();
    NightActions actions;
    actions.doctorHeal = 1;
    actions.commissarTarget = 1;
    actions.commissarShoots = true;
    NightResult r = resolveNight(players, actions);
    CHECK(r.deaths.empty());
    CHECK_EQ(r.healed, 1);
}

TEST(rules_doctor_saves_from_mafia_and_maniac) {
    auto players = samplePlayers();
    NightActions actions;
    actions.doctorHeal = 5;
    actions.mafiaTarget = 5;
    actions.maniacTarget = 5;
    NightResult r = resolveNight(players, actions);
    CHECK(r.deaths.empty());
    CHECK_EQ(r.savedTargets.size(), 1u);
}

TEST(rules_doctor_cannot_heal_same_twice) {
    auto players = samplePlayers();
    NightActions actions;
    actions.previousDoctorHeal = 5;
    actions.doctorHeal = 5;
    actions.mafiaTarget = 5;
    NightResult r = resolveNight(players, actions);
    CHECK(r.healRejected);
    CHECK_EQ(r.healed, kNoPlayer);
    CHECK_EQ(r.deaths.size(), 1u);
    CHECK_EQ(r.deaths.front().victim, 5);
    CHECK_EQ(r.deaths.front().cause, "убит мафией");
}

TEST(rules_mafia_and_maniac_different_targets) {
    auto players = samplePlayers();
    NightActions actions;
    actions.mafiaTarget = 5;
    actions.maniacTarget = 6;
    NightResult r = resolveNight(players, actions);
    CHECK_EQ(r.deaths.size(), 2u);
}

TEST(rules_commissar_check_sees_maniac_as_town) {
    auto players = samplePlayers();
    NightActions actions;
    actions.commissarTarget = 4;  // Маньяк
    actions.commissarShoots = false;
    NightResult r = resolveNight(players, actions);
    CHECK_EQ(r.commissarChecked, 4);
    CHECK(r.commissarSeenTeam.has_value());
    CHECK(*r.commissarSeenTeam == Team::Town);
    CHECK(r.deaths.empty());
}

TEST(rules_commissar_check_sees_mafia) {
    auto players = samplePlayers();
    NightActions actions;
    actions.commissarTarget = 1;
    NightResult r = resolveNight(players, actions);
    CHECK(*r.commissarSeenTeam == Team::Mafia);
}

TEST(rules_commissar_shoot_kills) {
    auto players = samplePlayers();
    NightActions actions;
    actions.commissarTarget = 1;
    actions.commissarShoots = true;
    NightResult r = resolveNight(players, actions);
    CHECK_EQ(r.deaths.size(), 1u);
    CHECK_EQ(r.deaths.front().victim, 1);
    CHECK_EQ(r.deaths.front().cause, "убит комиссаром");
    CHECK(!r.commissarSeenTeam.has_value());
}

TEST(rules_town_wins) {
    std::vector<PlayerInfo> players{
        makePlayer(1, Role::Civilian),
        makePlayer(2, Role::Doctor),
        makePlayer(3, Role::Mafia, false),
        makePlayer(4, Role::Maniac, false),
    };
    auto w = checkWinner(players);
    CHECK(w.has_value());
    CHECK(*w == Team::Town);
}

TEST(rules_mafia_wins_when_majority) {
    std::vector<PlayerInfo> players{
        makePlayer(1, Role::Mafia),
        makePlayer(2, Role::Mafia),
        makePlayer(3, Role::Civilian),
    };
    auto w = checkWinner(players);
    CHECK(w.has_value());
    CHECK(*w == Team::Mafia);
}

TEST(rules_mafia_wins_on_equality) {
    std::vector<PlayerInfo> players{
        makePlayer(1, Role::Mafia),
        makePlayer(2, Role::Civilian),
    };
    auto w = checkWinner(players);
    CHECK(w.has_value());
    CHECK(*w == Team::Mafia);
}

TEST(rules_equal_mafia_and_town_but_maniac_alive_continues) {
    // 1 мафия, 1 мирный, 1 маньяк: мафия == "остальные" по численности с мирными,
    // но по правилам при живом Маньяке игра продолжается.
    std::vector<PlayerInfo> players{
        makePlayer(1, Role::Mafia),
        makePlayer(2, Role::Civilian),
        makePlayer(3, Role::Maniac),
    };
    CHECK(!checkWinner(players).has_value());
}

TEST(rules_maniac_wins_one_on_one_with_town) {
    std::vector<PlayerInfo> players{
        makePlayer(1, Role::Maniac),
        makePlayer(2, Role::Civilian),
        makePlayer(3, Role::Mafia, false),
    };
    auto w = checkWinner(players);
    CHECK(w.has_value());
    CHECK(*w == Team::Maniac);
}

TEST(rules_maniac_wins_alone) {
    std::vector<PlayerInfo> players{
        makePlayer(1, Role::Maniac),
        makePlayer(2, Role::Civilian, false),
    };
    auto w = checkWinner(players);
    CHECK(w.has_value());
    CHECK(*w == Team::Maniac);
}

TEST(rules_maniac_vs_mafia_continues) {
    std::vector<PlayerInfo> players{
        makePlayer(1, Role::Maniac),
        makePlayer(2, Role::Mafia),
    };
    CHECK(!checkWinner(players).has_value());
}

TEST(rules_count_alive_by_team_uses_alive_only) {
    std::vector<PlayerInfo> players{
        makePlayer(1, Role::Mafia),
        makePlayer(2, Role::Mafia, false),
        makePlayer(3, Role::Civilian),
        makePlayer(4, Role::Maniac),
    };
    TeamCounts c = countAliveByTeam(players);
    CHECK_EQ(c.mafia, 1);
    CHECK_EQ(c.town, 1);
    CHECK_EQ(c.maniac, 1);
    CHECK_EQ(c.alive, 3);
}
