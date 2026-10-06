#include <game/brain.hpp>
#include <game/config.hpp>
#include <game/engine.hpp>
#include <game/host.hpp>
#include <game/logger.hpp>
#include <game/state.hpp>
#include <game/view.hpp>

#include <memory>
#include <sstream>

#include "test.hpp"

using namespace mafia;

namespace {

GameView tinyView(Role role) {
    GameView v;
    v.self = 1;
    v.myRole = role;
    v.day = 1;
    v.phase = Phase::Day;
    v.players = {{1, "Игрок1", true}, {2, "Игрок2", true}, {3, "Игрок3", true}};
    if (role == Role::Mafia) v.allies = {1, 2};
    return v;
}

}  // namespace

TEST(interactive_human_speak_and_vote) {
    // Днём человек только голосует (кого считает мафией); m/t не спрашиваем.
    std::istringstream in("2\n");
    std::ostringstream out;
    HumanBrain brain(in, out);
    std::mt19937 rng(1);

    auto view = tinyView(Role::Civilian);
    Statement s = brain.speak(view, rng);
    CHECK_EQ(s.speaker, 1);
    CHECK(s.text.find("слушает") != std::string::npos);

    PlayerId vote = brain.vote(view, rng);
    CHECK_EQ(vote, 2);
    CHECK(out.str().find("мафией") != std::string::npos);
    CHECK(out.str().find("m = мафия") == std::string::npos);
}

TEST(interactive_human_doctor_heal) {
    std::istringstream in("1\n");
    std::ostringstream out;
    HumanBrain brain(in, out);
    std::mt19937 rng(1);
    auto view = tinyView(Role::Doctor);
    view.phase = Phase::Night;
    auto d = brain.act(view, rng);
    CHECK(d.kind == NightKind::Heal);
    CHECK_EQ(d.target, 1);
}

TEST(interactive_human_commissar_check) {
    std::istringstream in("1\n2\n");
    std::ostringstream out;
    HumanBrain brain(in, out);
    std::mt19937 rng(1);
    auto view = tinyView(Role::Commissar);
    view.phase = Phase::Night;
    auto d = brain.act(view, rng);
    CHECK(d.kind == NightKind::Check);
    CHECK_EQ(d.target, 2);
}

TEST(interactive_fallback_on_bad_input) {
    std::istringstream in("999\n");
    std::ostringstream out;
    HumanBrain brain(in, out);
    std::mt19937 rng(1);
    auto view = tinyView(Role::Civilian);
    (void)brain.speak(view, rng);
    PlayerId vote = brain.vote(view, rng);
    CHECK(vote == 2 || vote == 3);  // fallback-бот выберет кого-то живого
    CHECK(out.str().find("бот") != std::string::npos);
}

TEST(interactive_piped_game_finishes) {
    std::istringstream in;  // пусто -> все ходы человека через fallback-бота
    std::ostringstream humanOut;
    auto human = smart::makeShared<HumanBrain>(in, humanOut);

    Config config;
    config.players = 6;
    config.seed = 3;
    config.seedGiven = true;
    config.interactive = true;
    config.fileLog = false;
    config.engine = EngineKind::Sequential;

    std::ostringstream sink;
    Logger logger(config, sink);
    Host host(config, std::make_unique<SequentialEngine>(), logger, human);
    auto outcome = host.run();
    CHECK(outcome.finished);
    CHECK(sink.str().find("Игрок1") != std::string::npos);
    CHECK(sink.str().find("интерактивный") != std::string::npos);
    CHECK(outcome.state->player(1).human);
}

TEST(interactive_marks_player_one_human) {
    Config config;
    config.players = 6;
    config.interactive = true;
    std::mt19937 rng(3);
    auto state = createGameState(config, defaultSpecialRoles(), rng);
    CHECK(state->player(1).human);
    CHECK(!state->player(2).human);
}
