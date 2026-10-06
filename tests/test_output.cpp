#include <game/config.hpp>
#include <game/engine.hpp>
#include <game/host.hpp>
#include <game/logger.hpp>

#include <memory>
#include <sstream>

#include "test.hpp"

using namespace mafia;

namespace {

struct RunResult {
    GameOutcome outcome;
    std::string log;
};

RunResult playCapture(int players, std::uint32_t seed, bool open, bool fullLog) {
    Config config;
    config.players = players;
    config.seed = seed;
    config.seedGiven = true;
    config.openAnnouncements = open;
    config.fullLog = fullLog;
    config.fileLog = false;
    config.engine = EngineKind::Sequential;

    std::ostringstream sink;
    Logger logger(config, sink);
    Host host(config, std::make_unique<SequentialEngine>(), logger);
    RunResult r;
    r.outcome = host.run();
    r.log = sink.str();
    return r;
}

}  // namespace

TEST(output_open_vs_closed_same_outcome) {
    auto open = playCapture(10, 42, true, false);
    auto closed = playCapture(10, 42, false, false);
    CHECK(open.outcome.finished);
    CHECK(closed.outcome.finished);
    CHECK(open.outcome.winner == closed.outcome.winner);
    CHECK_EQ(open.outcome.daysPlayed, closed.outcome.daysPlayed);
    CHECK(open.outcome.state->aliveIds() == closed.outcome.state->aliveIds());

    // Тексты объявлений различаются.
    CHECK(open.log.find("(Доктор)") != std::string::npos ||
          open.log.find("(Комиссар)") != std::string::npos ||
          open.log.find("(Мафия)") != std::string::npos ||
          open.log.find("(Маньяк)") != std::string::npos ||
          open.log.find("(Мирный житель)") != std::string::npos);
    CHECK(closed.log.find("[мирные]") != std::string::npos ||
          closed.log.find("[мафия]") != std::string::npos);
    // В закрытом режиме нет полных ролей в скобках и нет "убит мафией".
    CHECK(closed.log.find("убит мафией") == std::string::npos);
    CHECK(closed.log.find("убит маньяком") == std::string::npos);
    CHECK(closed.log.find("убит комиссаром") == std::string::npos);
}

TEST(output_full_log_contains_votes_and_night_moves) {
    auto r = playCapture(8, 7, false, true);
    CHECK(r.outcome.finished);
    CHECK(r.log.find("Голосование:") != std::string::npos);
    CHECK(r.log.find(" → ") != std::string::npos);
    CHECK(r.log.find("[лог]") != std::string::npos);
    CHECK(r.log.find("лечение") != std::string::npos || r.log.find("убийство") != std::string::npos ||
          r.log.find("проверка") != std::string::npos || r.log.find("пропуск") != std::string::npos);
    CHECK(r.log.find("говорит:") != std::string::npos);
}

TEST(output_brief_log_hides_detail_prefix) {
    auto r = playCapture(8, 7, false, false);
    CHECK(r.log.find("[лог]") == std::string::npos);
    CHECK(r.log.find("Голосование:") != std::string::npos);
}

TEST(logger_announce_helpers) {
    Config openCfg;
    openCfg.openAnnouncements = true;
    Config closedCfg;
    closedCfg.openAnnouncements = false;
    std::ostringstream sink;
    Logger open(openCfg, sink);
    Logger closed(closedCfg, sink);

    CHECK_EQ(open.announcePlayer("Игрок1", Role::Doctor), "Игрок1 (Доктор)");
    CHECK_EQ(closed.announcePlayer("Игрок1", Role::Doctor), "Игрок1 [мирные]");
    CHECK_EQ(closed.announcePlayer("Игрок2", Role::Maniac), "Игрок2 [мирные]");
    CHECK_EQ(closed.announcePlayer("Игрок3", Role::Mafia), "Игрок3 [мафия]");
    CHECK_EQ(open.announceCause("убит мафией"), "убит мафией");
    CHECK_EQ(closed.announceCause("убит мафией"), "убит ночью");
    CHECK_EQ(closed.announceCause("казнён"), "казнён");
    CHECK_EQ(Logger::nightKindName(NightKind::Check), "проверка");
}
