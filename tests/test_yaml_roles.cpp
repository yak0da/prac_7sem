#include <game/config.hpp>
#include <game/engine.hpp>
#include <game/host.hpp>
#include <game/logger.hpp>
#include <game/rules.hpp>
#include <game/state.hpp>
#include <game/yaml_lite.hpp>

#include <algorithm>
#include <memory>
#include <ranges>
#include <sstream>

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

int countRole(const std::vector<Role>& roles, Role role) {
    return static_cast<int>(std::ranges::count(roles, role));
}

}  // namespace

TEST(yaml_parse_roles_file) {
    const std::string text = R"(
mafia_divisor: 4
roles:
  - commissar
  - doctor
  - ninja
  - bodyguard
  - witness
  # комментарий
)";
    RolesFile file = parseRolesYaml(text);
    CHECK_EQ(file.mafiaDivisor, 4);
    CHECK_EQ(file.roles.size(), 5u);
    CHECK(file.roles[0] == Role::Commissar);
    CHECK(file.roles[2] == Role::Ninja);
    CHECK(file.roles[4] == Role::Witness);
}

TEST(yaml_unknown_role_throws) {
    bool threw = false;
    try {
        parseRolesYaml("roles:\n  - wizard\n");
    } catch (const YamlError&) {
        threw = true;
    }
    CHECK(threw);
}

TEST(yaml_load_project_roles_file) {
    RolesFile file = loadRolesYamlFile("config/roles.yaml");
    CHECK_EQ(file.mafiaDivisor, 3);
    CHECK(std::ranges::find(file.roles, Role::Ninja) != file.roles.end());
    CHECK(std::ranges::find(file.roles, Role::Bodyguard) != file.roles.end());
    CHECK(std::ranges::find(file.roles, Role::Witness) != file.roles.end());
}

TEST(model_ninja_counts_in_mafia_quota) {
    std::mt19937 rng(99);
    std::vector<Role> specials{Role::Commissar, Role::Doctor, Role::Maniac, Role::Ninja,
                               Role::Bodyguard, Role::Witness};
    auto roles = assignRoles(12, 3, specials, rng);
    CHECK_EQ(countRole(roles, Role::Ninja) + countRole(roles, Role::Mafia), mafiaCount(12, 3));
    CHECK_EQ(countRole(roles, Role::Ninja), 1);
    CHECK_EQ(countRole(roles, Role::Bodyguard), 1);
    CHECK_EQ(countRole(roles, Role::Witness), 1);
}

TEST(model_ninja_seen_as_town_by_commissar) {
    CHECK(teamOf(Role::Ninja) == Team::Mafia);
    CHECK(isMafiaRole(Role::Ninja));
    CHECK(seenByCommissar(Role::Ninja) == Team::Town);
    CHECK(teamOf(Role::Bodyguard) == Team::Town);
    CHECK(teamOf(Role::Witness) == Team::Town);
}

TEST(rules_commissar_check_sees_ninja_as_town) {
    std::vector<PlayerInfo> players{
        makePlayer(1, Role::Ninja),
        makePlayer(2, Role::Commissar),
        makePlayer(3, Role::Civilian),
        makePlayer(4, Role::Civilian),
        makePlayer(5, Role::Civilian),
    };
    NightActions actions;
    actions.commissarTarget = 1;
    actions.commissarShoots = false;
    NightResult r = resolveNight(players, actions);
    CHECK_EQ(r.commissarChecked, 1);
    CHECK(*r.commissarSeenTeam == Team::Town);
}

TEST(rules_bodyguard_saves_and_counters_mafia) {
    std::vector<PlayerInfo> players{
        makePlayer(1, Role::Mafia),
        makePlayer(2, Role::Bodyguard),
        makePlayer(3, Role::Civilian),
        makePlayer(4, Role::Civilian),
        makePlayer(5, Role::Civilian),
    };
    NightActions actions;
    actions.mafiaTarget = 3;
    actions.mafiaShooter = 1;
    actions.bodyguardProtect = 3;
    actions.bodyguardId = 2;
    NightResult r = resolveNight(players, actions);
    CHECK_EQ(r.bodyguardSaved, 3);
    CHECK_EQ(r.bodyguardKilled, 1);
    CHECK_EQ(r.deaths.size(), 1u);
    CHECK_EQ(r.deaths.front().victim, 1);
    CHECK_EQ(r.deaths.front().cause, "убит телохранителем");
}

TEST(rules_bodyguard_saves_from_commissar_no_counter) {
    std::vector<PlayerInfo> players{
        makePlayer(1, Role::Mafia),
        makePlayer(2, Role::Commissar),
        makePlayer(3, Role::Bodyguard),
        makePlayer(4, Role::Civilian),
        makePlayer(5, Role::Civilian),
    };
    NightActions actions;
    actions.commissarTarget = 1;
    actions.commissarShoots = true;
    actions.bodyguardProtect = 1;
    actions.bodyguardId = 3;
    NightResult r = resolveNight(players, actions);
    CHECK_EQ(r.bodyguardSaved, 1);
    CHECK_EQ(r.bodyguardKilled, kNoPlayer);
    CHECK(r.deaths.empty());
}

TEST(rules_witness_sees_attacker) {
    std::vector<PlayerInfo> players{
        makePlayer(1, Role::Mafia),
        makePlayer(2, Role::Witness),
        makePlayer(3, Role::Civilian),
        makePlayer(4, Role::Civilian),
        makePlayer(5, Role::Civilian),
    };
    NightActions actions;
    actions.mafiaTarget = 3;
    actions.mafiaShooter = 1;
    actions.witnessWatch = 3;
    actions.witnessId = 2;
    NightResult r = resolveNight(players, actions);
    CHECK_EQ(r.deaths.size(), 1u);
    CHECK_EQ(r.witnessReports.size(), 1u);
    CHECK_EQ(r.witnessReports.front().witness, 2);
    CHECK_EQ(r.witnessReports.front().watched, 3);
    CHECK_EQ(r.witnessReports.front().attacker, 1);
}

TEST(rules_witness_sees_even_if_doctor_saves) {
    std::vector<PlayerInfo> players{
        makePlayer(1, Role::Mafia),
        makePlayer(2, Role::Witness),
        makePlayer(3, Role::Doctor),
        makePlayer(4, Role::Civilian),
        makePlayer(5, Role::Civilian),
    };
    NightActions actions;
    actions.mafiaTarget = 4;
    actions.mafiaShooter = 1;
    actions.doctorHeal = 4;
    actions.witnessWatch = 4;
    actions.witnessId = 2;
    NightResult r = resolveNight(players, actions);
    CHECK(r.deaths.empty());
    CHECK_EQ(r.witnessReports.size(), 1u);
    CHECK_EQ(r.witnessReports.front().attacker, 1);
}

TEST(game_with_yaml_roles_finishes) {
    Config config;
    config.players = 12;
    config.seed = 7;
    config.seedGiven = true;
    config.rolesConfig = "config/roles.yaml";
    config.engine = EngineKind::Sequential;
    config.fileLog = false;
    config.fullLog = false;

    std::ostringstream sink;
    Logger logger(config, sink);
    Host host(config, std::make_unique<SequentialEngine>(), logger);
    GameOutcome outcome = host.run();
    CHECK(outcome.finished);
    CHECK(outcome.daysPlayed >= 1);
}
