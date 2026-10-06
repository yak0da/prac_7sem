#include <game/ai_brain.hpp>
#include <game/ai_client.hpp>
#include <game/config.hpp>
#include <game/engine.hpp>
#include <game/host.hpp>
#include <game/logger.hpp>
#include <game/yaml_lite.hpp>

#include <memory>
#include <sstream>

#include "test.hpp"

using namespace mafia;

namespace {

GameView sampleView() {
    GameView v;
    v.self = 2;
    v.myRole = Role::Civilian;
    v.day = 1;
    v.phase = Phase::Day;
    v.players = {{1, "Игрок1", true}, {2, "Игрок2", true}, {3, "Игрок3", true},
                 {4, "Игрок4", true}, {5, "Игрок5", true}};
    v.trust = {{1, -10}, {3, 5}, {4, -20}, {5, 0}};
    v.history = {"День 1 начался"};
    return v;
}

}  // namespace

TEST(ai_parse_valid_json_speak_and_vote) {
    auto client = std::make_shared<FakeAiClient>([](const std::string&, const std::string& user) {
        if (user.find("Проголосуй") != std::string::npos) {
            return std::optional<std::string>(
                R"({"action":"vote","message":"","target":4,"reasoning":"низкое доверие"})");
        }
        return std::optional<std::string>(
            R"({"action":"accuse","message":"Игрок4 ведёт себя странно","target":4,"reasoning":"trust"})");
    });
    auto budget = std::make_shared<AiBudget>(10);
    auto transcript = std::make_shared<AiTranscript>();
    AiBrain brain(client, budget, transcript, "никому не доверяет");
    std::mt19937 rng(1);
    auto view = sampleView();

    Statement s = brain.speak(view, rng);
    CHECK_EQ(s.about, 4);
    CHECK(s.claim == Team::Mafia);
    CHECK(s.text.find("странно") != std::string::npos);

    PlayerId vote = brain.vote(view, rng);
    CHECK_EQ(vote, 4);
    CHECK_EQ(client->calls(), 2);
    CHECK_EQ(budget->used(), 2);
}

TEST(ai_invalid_json_falls_back) {
    auto client = std::make_shared<FakeAiClient>(
        [](const std::string&, const std::string&) { return std::optional<std::string>("not json"); });
    auto budget = std::make_shared<AiBudget>(10);
    AiBrain brain(client, budget, nullptr, "тест");
    std::mt19937 rng(2);
    auto view = sampleView();
    // fallback не падает и возвращает кого-то живого
    Statement s = brain.speak(view, rng);
    CHECK(s.about != kNoPlayer);
    CHECK(s.about != view.self);
}

TEST(ai_bad_target_falls_back) {
    auto client = std::make_shared<FakeAiClient>([](const std::string&, const std::string&) {
        return std::optional<std::string>(
            R"({"action":"vote","message":"","target":99,"reasoning":"x"})");
    });
    auto budget = std::make_shared<AiBudget>(10);
    AiBrain brain(client, budget, nullptr, "тест");
    std::mt19937 rng(3);
    PlayerId vote = brain.vote(sampleView(), rng);
    CHECK(vote >= 1 && vote <= 5);
    CHECK(vote != 2);
}

TEST(ai_budget_limit_per_round) {
    int calls = 0;
    auto client = std::make_shared<FakeAiClient>([&](const std::string&, const std::string&) {
        ++calls;
        return std::optional<std::string>(
            R"({"action":"vote","message":"","target":4,"reasoning":"x"})");
    });
    auto budget = std::make_shared<AiBudget>(2);
    AiBrain brain(client, budget, nullptr, "тест");
    std::mt19937 rng(4);
    auto view = sampleView();
    CHECK_EQ(brain.vote(view, rng), 4);
    CHECK_EQ(brain.vote(view, rng), 4);
    // третий — лимит, fallback (всё равно валидный голос)
    PlayerId third = brain.vote(view, rng);
    CHECK(third != kNoPlayer);
    CHECK_EQ(budget->used(), 2);
    CHECK_EQ(client->calls(), 2);

    budget->resetRound();
    CHECK_EQ(brain.vote(view, rng), 4);
    CHECK_EQ(client->calls(), 3);
}

TEST(ai_night_doctor_heal) {
    auto client = std::make_shared<FakeAiClient>([](const std::string&, const std::string&) {
        return std::optional<std::string>(
            R"({"action":"heal","message":"","target":2,"reasoning":"себя"})");
    });
    auto budget = std::make_shared<AiBudget>(10);
    AiBrain brain(client, budget, nullptr, "осторожный");
    std::mt19937 rng(5);
    GameView v = sampleView();
    v.self = 2;
    v.myRole = Role::Doctor;
    auto d = brain.act(v, rng);
    CHECK(d.kind == NightKind::Heal);
    CHECK_EQ(d.target, 2);
}

TEST(yaml_characters_parsed) {
    const std::string text = R"(
roles:
  - doctor
characters:
  - "плохо врет"
  - 'никому не доверяет'
)";
    RolesFile file = parseRolesYaml(text);
    CHECK_EQ(file.characters.size(), 2u);
    CHECK_EQ(file.characters[0], "плохо врет");
    CHECK_EQ(file.characters[1], "никому не доверяет");
}

TEST(game_with_ai_fake_client_finishes) {
    Config config;
    config.players = 8;
    config.seed = 11;
    config.seedGiven = true;
    config.aiPlayers = 2;
    config.engine = EngineKind::Sequential;
    config.fileLog = false;
    config.rolesConfig = "config/roles.yaml";

    auto client = std::make_shared<FakeAiClient>([](const std::string&, const std::string& user) {
        if (user.find("Ночной") != std::string::npos || user.find("night") != std::string::npos ||
            user.find("Ночной ход") != std::string::npos) {
            // ночью часто role-specific — пусть чаще уходит в fallback через skip-invalid
            return std::optional<std::string>(
                R"({"action":"skip","message":"","target":null,"reasoning":"bot"})");
        }
        if (user.find("Проголосуй") != std::string::npos) {
            return std::optional<std::string>(
                R"({"action":"vote","message":"","target":3,"reasoning":"random"})");
        }
        return std::optional<std::string>(
            R"({"action":"accuse","message":"Подозреваю Игрок3","target":3,"reasoning":"ai"})");
    });

    std::ostringstream sink;
    Logger logger(config, sink);
    Host host(config, std::make_unique<SequentialEngine>(), logger);
    host.setAiClient(client);
    GameOutcome outcome = host.run();
    CHECK(outcome.finished);
    CHECK(client->calls() > 0);
    CHECK(!outcome.aiTranscriptPath.empty());
}
