#include <game/ai_brain.hpp>

#include <json.hpp>

#include <algorithm>
#include <cctype>
#include <sstream>

namespace mafia {

namespace {

const char* kSystemPrompt =
    "Ты игрок в настольной игре «Мафия». Отвечай ТОЛЬКО одним JSON-объектом без markdown:\n"
    "{\"action\":\"...\",\"message\":\"...\",\"target\":<номер или null>,\"reasoning\":\"...\"}\n"
    "action: speak|accuse|defend|vote|heal|kill|check|shoot|protect|watch|skip\n"
    "target — номер живого игрока (или себя для heal/protect). Не называй чужие скрытые роли.\n"
    "message — короткая реплика на русском (для дня). reasoning — краткое обоснование.";

std::string nameOf(const GameView& view, PlayerId id) {
    for (const auto& p : view.players) {
        if (p.id == id) return p.name;
    }
    return "Игрок" + std::to_string(id);
}

}  // namespace

AiBrain::AiBrain(std::shared_ptr<AiClient> client, std::shared_ptr<AiBudget> budget,
                 std::shared_ptr<AiTranscript> transcript, std::string character)
    : client_(std::move(client)),
      budget_(std::move(budget)),
      transcript_(std::move(transcript)),
      character_(std::move(character)) {
    if (character_.empty()) character_ = "осторожный игрок";
}

std::string AiBrain::buildUserPrompt(const GameView& view, const std::string& phase,
                                     const std::string& task) const {
    std::ostringstream out;
    out << "Фаза: " << phase << " (день " << view.day << ")\n";
    out << "Задача: " << task << "\n";
    out << "Ты: " << nameOf(view, view.self) << " (id=" << view.self << ")\n";
    out << "Роль: " << roleName(view.myRole) << " [" << teamName(teamOf(view.myRole)) << "]\n";
    out << "Характер: " << character_ << "\n";

    if (!view.allies.empty()) {
        out << "Сообщники:";
        for (PlayerId id : view.allies) {
            if (id == view.self) continue;
            out << " " << nameOf(view, id) << "(id=" << id << ")";
        }
        out << "\n";
    }
    if (!view.checks.empty()) {
        out << "Твои проверки:";
        for (const auto& [id, team] : view.checks) {
            out << " " << nameOf(view, id) << "=" << teamName(team);
        }
        out << "\n";
    }
    if (view.previousHeal != kNoPlayer) {
        out << "Нельзя снова лечить: " << nameOf(view, view.previousHeal) << "\n";
    }

    out << "Живые игроки:\n";
    for (const auto& p : view.players) {
        if (!p.alive) continue;
        out << "  id=" << p.id << " " << p.name;
        if (p.id == view.self) out << " (ты)";
        if (view.trust.contains(p.id)) out << " trust=" << view.trust.at(p.id);
        out << "\n";
    }

    if (!view.history.empty()) {
        out << "История недавних событий:\n";
        const std::size_t from = view.history.size() > 20 ? view.history.size() - 20 : 0;
        for (std::size_t i = from; i < view.history.size(); ++i) {
            out << "  - " << view.history[i] << "\n";
        }
    }
    out << "Верни только JSON.";
    return out.str();
}

std::string AiBrain::extractJsonObject(const std::string& text) {
    const auto start = text.find('{');
    const auto end = text.rfind('}');
    if (start == std::string::npos || end == std::string::npos || end <= start) return {};
    return text.substr(start, end - start + 1);
}

AiBrain::ParsedReply AiBrain::parseReply(const std::string& raw) {
    ParsedReply r;
    const std::string jsonText = extractJsonObject(raw);
    if (jsonText.empty()) return r;
    try {
        auto j = nlohmann::json::parse(jsonText);
        if (j.contains("action") && j["action"].is_string()) r.action = j["action"].get<std::string>();
        if (j.contains("message") && j["message"].is_string()) r.message = j["message"].get<std::string>();
        if (j.contains("reasoning") && j["reasoning"].is_string()) {
            r.reasoning = j["reasoning"].get<std::string>();
        }
        if (j.contains("target") && !j["target"].is_null()) {
            if (j["target"].is_number_integer()) r.target = j["target"].get<int>();
            else if (j["target"].is_string()) {
                try {
                    r.target = std::stoi(j["target"].get<std::string>());
                } catch (...) {
                    r.target = kNoPlayer;
                }
            }
        }
        // нормализуем action
        for (char& c : r.action) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        r.ok = !r.action.empty();
    } catch (...) {
        r.ok = false;
    }
    return r;
}

bool AiBrain::validTarget(const GameView& view, PlayerId target, bool allowSelf) const {
    if (target == kNoPlayer) return false;
    if (!allowSelf && target == view.self) return false;
    for (const auto& p : view.players) {
        if (p.id == target && p.alive) return true;
    }
    return false;
}

AiBrain::ParsedReply AiBrain::ask(const GameView& view, const std::string& phase,
                                  const std::string& task, std::mt19937& /*rng*/) {
    ParsedReply fail;
    if (!client_ || !client_->ready()) {
        if (transcript_) transcript_->append(phase, view.self, "(no api)", "(fallback)", true);
        return fail;
    }
    if (!budget_ || !budget_->tryConsume()) {
        if (transcript_) transcript_->append(phase, view.self, "(budget)", "(fallback)", true);
        return fail;
    }

    const std::string user = buildUserPrompt(view, phase, task);
    auto reply = client_->complete(kSystemPrompt, user);
    if (!reply) {
        if (transcript_) transcript_->append(phase, view.self, user, "(no response)", true);
        return fail;
    }

    ParsedReply parsed = parseReply(*reply);
    if (transcript_) {
        transcript_->append(phase, view.self, user, *reply, !parsed.ok);
    }
    return parsed;
}

Statement AiBrain::speak(const GameView& view, std::mt19937& rng) {
    auto parsed = ask(view, "day_speak",
                      "Сделай дневную реплику. action=speak|accuse|defend, target=кого обсуждаешь.",
                      rng);
    if (!parsed.ok || !validTarget(view, parsed.target, false)) {
        return fallback_.speak(view, rng);
    }

    Statement s;
    s.speaker = view.self;
    s.about = parsed.target;
    if (parsed.action == "defend") s.claim = Team::Town;
    else s.claim = Team::Mafia;  // accuse / speak по умолчанию обвинение

    if (!parsed.message.empty()) {
        s.text = parsed.message;
    } else {
        s.text = nameOf(view, view.self) + (s.claim == Team::Mafia ? " обвиняет " : " защищает ") +
                 nameOf(view, s.about);
    }
    return s;
}

PlayerId AiBrain::vote(const GameView& view, std::mt19937& rng) {
    auto parsed =
        ask(view, "day_vote", "Проголосуй. action=vote, target=номер против кого голосуешь.", rng);
    if (!parsed.ok || parsed.action != "vote" || !validTarget(view, parsed.target, false)) {
        return fallback_.vote(view, rng);
    }
    // мафия не должна голосовать против своих, если ИИ ошибся
    if (isMafiaRole(view.myRole) &&
        std::ranges::find(view.allies, parsed.target) != view.allies.end()) {
        return fallback_.vote(view, rng);
    }
    return parsed.target;
}

PlayerNightDecision AiBrain::act(const GameView& view, std::mt19937& rng) {
    if (view.myRole == Role::Civilian) {
        return PlayerNightDecision{NightKind::Skip, kNoPlayer};
    }

    auto parsed = ask(view, "night",
                      "Ночной ход по своей роли. action=heal|kill|check|shoot|protect|watch|skip.",
                      rng);
    if (!parsed.ok) return fallback_.act(view, rng);

    PlayerNightDecision d;
    if (parsed.action == "heal") d.kind = NightKind::Heal;
    else if (parsed.action == "kill") d.kind = NightKind::Kill;
    else if (parsed.action == "check") d.kind = NightKind::Check;
    else if (parsed.action == "shoot") d.kind = NightKind::Shoot;
    else if (parsed.action == "protect") d.kind = NightKind::Protect;
    else if (parsed.action == "watch") d.kind = NightKind::Watch;
    else if (parsed.action == "skip") return PlayerNightDecision{NightKind::Skip, kNoPlayer};
    else return fallback_.act(view, rng);

    const bool allowSelf = (d.kind == NightKind::Heal || d.kind == NightKind::Protect);
    if (!validTarget(view, parsed.target, allowSelf)) return fallback_.act(view, rng);

    // роли не должны выдавать чужие действия
    switch (view.myRole) {
        case Role::Doctor:
            if (d.kind != NightKind::Heal) return fallback_.act(view, rng);
            if (parsed.target == view.previousHeal) return fallback_.act(view, rng);
            break;
        case Role::Commissar:
            if (d.kind != NightKind::Check && d.kind != NightKind::Shoot) return fallback_.act(view, rng);
            break;
        case Role::Mafia:
        case Role::Ninja:
        case Role::Maniac:
            if (d.kind != NightKind::Kill) return fallback_.act(view, rng);
            break;
        case Role::Bodyguard:
            if (d.kind != NightKind::Protect) return fallback_.act(view, rng);
            break;
        case Role::Witness:
            if (d.kind != NightKind::Watch) return fallback_.act(view, rng);
            break;
        case Role::Civilian:
            break;
    }

    d.target = parsed.target;
    return d;
}

}  // namespace mafia
