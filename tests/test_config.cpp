#include <game/config.hpp>

#include <vector>

#include "test.hpp"

using namespace mafia;

namespace {

Config parse(std::vector<const char*> args) {
    args.insert(args.begin(), "mafia");
    return parseArgs(static_cast<int>(args.size()), args.data());
}

bool fails(std::vector<const char*> args) {
    try {
        parse(std::move(args));
    } catch (const ConfigError&) {
        return true;
    }
    return false;
}

}  // namespace

TEST(config_defaults) {
    Config c = parse({});
    CHECK_EQ(c.players, 10);
    CHECK_EQ(c.mafiaDivisor, 3);
    CHECK(!c.interactive);
    CHECK(!c.openAnnouncements);
    CHECK(!c.fullLog);
    CHECK(!c.seedGiven);
    CHECK(c.engine == EngineKind::Thread);
    CHECK(c.fileLog);
}

TEST(config_example_from_task) {
    Config c = parse({"--players", "10", "--interactive", "--open-announcements", "--full-log"});
    CHECK_EQ(c.players, 10);
    CHECK(c.interactive);
    CHECK(c.openAnnouncements);
    CHECK(c.fullLog);
}

TEST(config_all_options) {
    Config c = parse({"-n", "7", "-k", "4", "--seed", "42", "--engine", "coro", "--config", "r.yaml",
                      "--log-dir", "out", "--no-file-log", "--ai", "2", "--closed-announcements"});
    CHECK_EQ(c.players, 7);
    CHECK_EQ(c.mafiaDivisor, 4);
    CHECK_EQ(c.seed, 42u);
    CHECK(c.seedGiven);
    CHECK(c.engine == EngineKind::Coroutine);
    CHECK_EQ(c.rolesConfig, "r.yaml");
    CHECK_EQ(c.logDir, "out");
    CHECK(!c.fileLog);
    CHECK_EQ(c.aiPlayers, 2);
    CHECK(!c.openAnnouncements);
    CHECK(parse({"--engine", "seq"}).engine == EngineKind::Sequential);
}

TEST(config_errors) {
    CHECK(fails({"--players", "4"}));
    CHECK(fails({"--players"}));
    CHECK(fails({"--players", "ten"}));
    CHECK(fails({"--players", "10x"}));
    CHECK(fails({"-k", "2"}));
    CHECK(fails({"--engine", "gpu"}));
    CHECK(fails({"--unknown"}));
    CHECK(fails({"--ai", "11"}));
    CHECK(!fails({"--help", "--players", "1"}));
}

TEST(config_describe_mentions_values) {
    Config c = parse({"--players", "12", "--seed", "5", "--open-announcements"});
    std::string text = describe(c);
    CHECK(text.find("12") != std::string::npos);
    CHECK(text.find("open") != std::string::npos);
    CHECK(text.find("5") != std::string::npos);
}
