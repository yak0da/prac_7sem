#include <game/concepts.hpp>
#include <game/config.hpp>
#include <game/engine.hpp>
#include <game/file_logger.hpp>
#include <game/host.hpp>
#include <game/logger.hpp>

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>

#include "test.hpp"

using namespace mafia;
namespace fs = std::filesystem;

namespace {

struct NotAPlayer {};

GameOutcome playWithLogs(const fs::path& logDir, std::uint32_t seed) {
    Config config;
    config.players = 8;
    config.seed = seed;
    config.seedGiven = true;
    config.fileLog = true;
    config.logDir = logDir.string();
    config.engine = EngineKind::Sequential;
    config.fullLog = true;

    std::ostringstream sink;
    Logger logger(config, sink);
    Host host(config, std::make_unique<SequentialEngine>(), logger);
    return host.run();
}

}  // namespace

TEST(concepts_roles_satisfy) {
    assertRoleConcepts();
    CHECK(PlayerLike<Doctor>);
    CHECK(NightActor<MafiaPlayer>);
    CHECK(Healer<Doctor>);
    CHECK(Investigator<Commissar>);
    CHECK(!Healer<Civilian>);
    CHECK(!Investigator<Maniac>);
    CHECK((!PlayerLike<NotAPlayer>));
    CHECK((!NightActor<NotAPlayer>));
}

TEST(filesystem_writes_rounds_and_summary) {
    const fs::path base = fs::temp_directory_path() / "mafia_test_logs";
    fs::remove_all(base);

    auto outcome = playWithLogs(base, 11);
    CHECK(outcome.finished);
    CHECK(!outcome.logDirectory.empty());
    CHECK(fs::exists(outcome.logDirectory));
    CHECK(fs::exists(outcome.logDirectory / "summary.txt"));

    int roundFiles = 0;
    for (const auto& entry : fs::directory_iterator(outcome.logDirectory)) {
        const auto name = entry.path().filename().string();
        if (name.starts_with("round_") && name.ends_with(".txt")) ++roundFiles;
    }
    CHECK(roundFiles >= 1);
    CHECK(roundFiles >= outcome.daysPlayed);  // round_00 + дни, либо только дни

    std::ifstream summary(outcome.logDirectory / "summary.txt");
    std::string content((std::istreambuf_iterator<char>(summary)), std::istreambuf_iterator<char>());
    CHECK(content.find("Победитель:") != std::string::npos);
    CHECK(content.find("Игрок1") != std::string::npos);
    CHECK(content.find("роль=") != std::string::npos);

    // В файле раунда есть голосование.
    bool foundVote = false;
    for (const auto& entry : fs::directory_iterator(outcome.logDirectory)) {
        const auto name = entry.path().filename().string();
        if (!name.starts_with("round_") || name == "round_00.txt") continue;
        std::ifstream in(entry.path());
        std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        if (text.find("Голосование:") != std::string::npos) foundVote = true;
    }
    CHECK(foundVote);

    fs::remove_all(base);
}

TEST(filesystem_disabled_creates_nothing) {
    Config config;
    config.players = 6;
    config.seed = 1;
    config.seedGiven = true;
    config.fileLog = false;
    config.engine = EngineKind::Sequential;

    std::ostringstream sink;
    Logger logger(config, sink);
    Host host(config, std::make_unique<SequentialEngine>(), logger);
    auto outcome = host.run();
    CHECK(outcome.finished);
    CHECK(outcome.logDirectory.empty() || !fs::exists(outcome.logDirectory) ||
          outcome.logDirectory.filename().string().find("game_") == std::string::npos);
}
