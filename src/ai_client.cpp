#include <game/ai_client.hpp>

#include <json.hpp>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include <curl/curl.h>

namespace mafia {

namespace {

std::string envOr(const char* name, const char* fallback) {
    if (const char* v = std::getenv(name); v && *v) return v;
    return fallback ? fallback : "";
}

size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

}  // namespace

AiSettings AiSettings::fromEnv() {
    AiSettings s;
    s.apiKey = envOr("MAFIA_AI_KEY", "");
    s.apiUrl = envOr("MAFIA_AI_URL", "https://api.openai.com/v1/chat/completions");
    s.model = envOr("MAFIA_AI_MODEL", "gpt-4o-mini");
    return s;
}

AiBudget::AiBudget(int maxPerRound) : maxPerRound_(maxPerRound) {}

void AiBudget::resetRound() {
    std::lock_guard lock(mutex_);
    used_ = 0;
}

bool AiBudget::tryConsume() {
    std::lock_guard lock(mutex_);
    if (used_ >= maxPerRound_) return false;
    ++used_;
    return true;
}

int AiBudget::used() const {
    std::lock_guard lock(mutex_);
    return used_;
}

int AiBudget::remaining() const {
    std::lock_guard lock(mutex_);
    return std::max(0, maxPerRound_ - used_);
}

void AiTranscript::append(const std::string& phase, PlayerId player, const std::string& prompt,
                          const std::string& response, bool usedFallback) {
    nlohmann::json j;
    j["phase"] = phase;
    j["player"] = player;
    j["prompt"] = prompt;
    j["response"] = response;
    j["fallback"] = usedFallback;
    std::lock_guard lock(mutex_);
    lines_.push_back(j.dump());
}

void AiTranscript::writeToFile(const std::string& path) const {
    std::lock_guard lock(mutex_);
    std::ofstream out(path);
    for (const auto& line : lines_) out << line << "\n";
}

CurlAiClient::CurlAiClient(AiSettings settings) : settings_(std::move(settings)) {}

std::optional<std::string> CurlAiClient::complete(const std::string& systemPrompt,
                                                  const std::string& userPrompt) {
    if (!settings_.configured()) return std::nullopt;

    std::lock_guard lock(mutex_);

    nlohmann::json body;
    body["model"] = settings_.model;
    body["temperature"] = 0.4;
    body["messages"] = nlohmann::json::array(
        {{{"role", "system"}, {"content", systemPrompt}},
         {{"role", "user"}, {"content", userPrompt}}});

    const std::string payload = body.dump();
    std::string response;

    CURL* curl = curl_easy_init();
    if (!curl) return std::nullopt;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    const std::string auth = "Authorization: Bearer " + settings_.apiKey;
    headers = curl_slist_append(headers, auth.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, settings_.apiUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(payload.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(settings_.timeoutSec));

    const CURLcode code = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (code != CURLE_OK || httpCode < 200 || httpCode >= 300) return std::nullopt;

    try {
        auto parsed = nlohmann::json::parse(response);
        return parsed.at("choices").at(0).at("message").at("content").get<std::string>();
    } catch (...) {
        return std::nullopt;
    }
}

}  // namespace mafia
