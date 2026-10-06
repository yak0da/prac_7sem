#pragma once

#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "types.hpp"

namespace mafia {

// Настройки OpenAI-совместимого API (из переменных окружения).
struct AiSettings {
    std::string apiKey;     // MAFIA_AI_KEY
    std::string apiUrl;     // MAFIA_AI_URL
    std::string model;      // MAFIA_AI_MODEL
    int timeoutSec = 30;

    static AiSettings fromEnv();
    bool configured() const { return !apiKey.empty(); }
};

// Абстрактный клиент LLM: один запрос system+user -> текст ответа.
class AiClient {
public:
    virtual ~AiClient() = default;
    // false — нет ключа/клиента; вызовы не считаются в лимит, сразу fallback.
    virtual bool ready() const { return true; }
    virtual std::optional<std::string> complete(const std::string& systemPrompt,
                                                const std::string& userPrompt) = 0;
};

// Реальный HTTP-клиент через libcurl (OpenAI Chat Completions).
class CurlAiClient final : public AiClient {
public:
    explicit CurlAiClient(AiSettings settings);
    bool ready() const override { return settings_.configured(); }
    std::optional<std::string> complete(const std::string& systemPrompt,
                                        const std::string& userPrompt) override;

private:
    AiSettings settings_;
    std::mutex mutex_;
};

// Лимит запросов за игровой раунд (день+ночь).
class AiBudget {
public:
    explicit AiBudget(int maxPerRound = 10);

    void resetRound();
    bool tryConsume();  // false, если лимит исчерпан
    int used() const;
    int remaining() const;
    int maxPerRound() const { return maxPerRound_; }

private:
    int maxPerRound_;
    int used_ = 0;
    mutable std::mutex mutex_;
};

// Журнал диалогов с ИИ (для сдачи артефактов трека).
class AiTranscript {
public:
    void append(const std::string& phase, PlayerId player, const std::string& prompt,
                const std::string& response, bool usedFallback);
    void writeToFile(const std::string& path) const;
    const std::vector<std::string>& lines() const { return lines_; }
    void clear() { lines_.clear(); }

private:
    std::vector<std::string> lines_;
    mutable std::mutex mutex_;
};

// Для тестов: подставляемый ответ / счётчик вызовов.
class FakeAiClient final : public AiClient {
public:
    using Handler = std::function<std::optional<std::string>(const std::string& system,
                                                             const std::string& user)>;

    explicit FakeAiClient(Handler handler) : handler_(std::move(handler)) {}

    std::optional<std::string> complete(const std::string& systemPrompt,
                                        const std::string& userPrompt) override {
        ++calls_;
        return handler_(systemPrompt, userPrompt);
    }

    int calls() const { return calls_; }

private:
    Handler handler_;
    int calls_ = 0;
};

}  // namespace mafia
