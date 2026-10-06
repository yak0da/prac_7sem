#pragma once

#include <memory>
#include <string>

#include "ai_client.hpp"
#include "brain.hpp"

namespace mafia {

// Мозг на LLM: собирает промпт, ждёт JSON, при сбое/лимите — BotBrain.
class AiBrain final : public Brain {
public:
    AiBrain(std::shared_ptr<AiClient> client, std::shared_ptr<AiBudget> budget,
            std::shared_ptr<AiTranscript> transcript, std::string character);

    Statement speak(const GameView& view, std::mt19937& rng) override;
    PlayerId vote(const GameView& view, std::mt19937& rng) override;
    PlayerNightDecision act(const GameView& view, std::mt19937& rng) override;

    const std::string& character() const { return character_; }

private:
    struct ParsedReply {
        std::string action;
        std::string message;
        PlayerId target = kNoPlayer;
        std::string reasoning;
        bool ok = false;
    };

    ParsedReply ask(const GameView& view, const std::string& phase, const std::string& task,
                    std::mt19937& rng);
    std::string buildUserPrompt(const GameView& view, const std::string& phase,
                                const std::string& task) const;
    static ParsedReply parseReply(const std::string& raw);
    static std::string extractJsonObject(const std::string& text);
    bool validTarget(const GameView& view, PlayerId target, bool allowSelf) const;

    std::shared_ptr<AiClient> client_;
    std::shared_ptr<AiBudget> budget_;
    std::shared_ptr<AiTranscript> transcript_;
    std::string character_;
    BotBrain fallback_;
};

}  // namespace mafia
