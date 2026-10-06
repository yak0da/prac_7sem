#include <game/brain.hpp>

#include <algorithm>
#include <ranges>
#include <sstream>
#include <vector>

namespace mafia {

namespace {

std::vector<PlayerId> aliveOthers(const GameView& view) {
    std::vector<PlayerId> ids;
    for (const auto& p : view.players) {
        if (p.alive && p.id != view.self) ids.push_back(p.id);
    }
    return ids;
}

bool isAlly(const GameView& view, PlayerId id) {
    return std::ranges::find(view.allies, id) != view.allies.end();
}

// Кандидаты: живые, не я, не союзники. Сортировка: меньше доверие - раньше.
std::vector<PlayerId> suspects(const GameView& view) {
    auto ids = aliveOthers(view);
    std::erase_if(ids, [&](PlayerId id) { return isAlly(view, id); });
    std::ranges::sort(ids, [&](PlayerId a, PlayerId b) {
        const int ta = view.trust.contains(a) ? view.trust.at(a) : 0;
        const int tb = view.trust.contains(b) ? view.trust.at(b) : 0;
        if (ta != tb) return ta < tb;
        return a < b;
    });
    return ids;
}

PlayerId pickFrom(const std::vector<PlayerId>& ids, std::mt19937& rng) {
    if (ids.empty()) return kNoPlayer;
    // Чаще берём самых подозрительных, иногда случайного среди топ-3.
    const std::size_t top = std::min<std::size_t>(3, ids.size());
    std::uniform_int_distribution<std::size_t> dist(0, top - 1);
    return ids[dist(rng)];
}

std::string nameOf(const GameView& view, PlayerId id) {
    for (const auto& p : view.players) {
        if (p.id == id) return p.name;
    }
    return "Игрок" + std::to_string(id);
}

}  // namespace

Statement BotBrain::speak(const GameView& view, std::mt19937& rng) {
    Statement s;
    s.speaker = view.self;
    auto list = suspects(view);
    if (list.empty()) list = aliveOthers(view);
    if (list.empty()) {
        s.about = view.self;
        s.claim = Team::Town;
        s.text = nameOf(view, view.self) + " говорит: «я не мафия»";
        return s;
    }

    // Мафия иногда врёт и обвиняет мирного; мирные обвиняют наименее доверенного.
    s.about = pickFrom(list, rng);
    s.claim = Team::Mafia;

    // Комиссар с подтверждённой мафией говорит о ней.
    if (view.myRole == Role::Commissar) {
        for (const auto& [id, team] : view.checks) {
            if (team == Team::Mafia) {
                for (const auto& p : view.players) {
                    if (p.id == id && p.alive) {
                        s.about = id;
                        s.claim = Team::Mafia;
                        break;
                    }
                }
            }
        }
    }

    // Мафия иногда защищает сообщника ("он мирный").
    if (view.myRole == Role::Mafia && !view.allies.empty() &&
        std::uniform_int_distribution<int>(0, 2)(rng) == 0) {
        PlayerId ally = view.allies.front();
        for (PlayerId a : view.allies) {
            if (a != view.self) {
                ally = a;
                break;
            }
        }
        for (const auto& p : view.players) {
            if (p.id == ally && p.alive) {
                s.about = ally;
                s.claim = Team::Town;
                break;
            }
        }
    }

    std::ostringstream text;
    text << nameOf(view, view.self) << " говорит: «" << nameOf(view, s.about) << " — "
         << (s.claim == Team::Mafia ? "мафия" : "мирный") << "»";
    s.text = text.str();
    return s;
}

PlayerId BotBrain::vote(const GameView& view, std::mt19937& rng) {
    // Комиссар голосует против проверенной мафии.
    if (view.myRole == Role::Commissar) {
        for (const auto& [id, team] : view.checks) {
            if (team != Team::Mafia) continue;
            for (const auto& p : view.players) {
                if (p.id == id && p.alive && id != view.self) return id;
            }
        }
    }

    // Голосуем за того, кого только что обвинили в обсуждении (если цель ещё жива и не союзник).
    if (view.lastAccused != kNoPlayer && view.lastAccused != view.self) {
        for (const auto& p : view.players) {
            if (p.id == view.lastAccused && p.alive && !isAlly(view, p.id)) {
                return view.lastAccused;
            }
        }
    }

    auto list = suspects(view);
    if (list.empty()) list = aliveOthers(view);
    return pickFrom(list, rng);
}

PlayerNightDecision BotBrain::act(const GameView& view, std::mt19937& rng) {
    PlayerNightDecision d;
    auto list = suspects(view);
    auto others = aliveOthers(view);

    switch (view.myRole) {
        case Role::Civilian:
            d.kind = NightKind::Skip;
            break;

        case Role::Doctor: {
            d.kind = NightKind::Heal;
            std::vector<PlayerId> candidates = others;
            // Можно лечить себя.
            candidates.push_back(view.self);
            std::erase_if(candidates, [&](PlayerId id) { return id == view.previousHeal; });
            if (candidates.empty()) candidates = others;
            // Чаще лечим себя или наименее доверенного "важного" - эвристика: лечим себя
            // с вероятностью 1/3, иначе случайного из кандидатов с приоритетом низкого доверия.
            if (!candidates.empty() && std::uniform_int_distribution<int>(0, 2)(rng) == 0 &&
                view.self != view.previousHeal) {
                d.target = view.self;
            } else {
                std::ranges::sort(candidates, [&](PlayerId a, PlayerId b) {
                    if (a == view.self) return false;
                    if (b == view.self) return true;
                    const int ta = view.trust.contains(a) ? view.trust.at(a) : 0;
                    const int tb = view.trust.contains(b) ? view.trust.at(b) : 0;
                    // Высокое доверие = скорее свой, лечим его.
                    if (ta != tb) return ta > tb;
                    return a < b;
                });
                d.target = pickFrom(candidates, rng);
                if (d.target == kNoPlayer && !candidates.empty()) d.target = candidates.front();
            }
            break;
        }

        case Role::Commissar: {
            // Стреляем в проверенную мафию, иначе проверяем подозрительного без проверки.
            PlayerId knownMafia = kNoPlayer;
            for (const auto& [id, team] : view.checks) {
                if (team != Team::Mafia) continue;
                for (const auto& p : view.players) {
                    if (p.id == id && p.alive) knownMafia = id;
                }
            }
            if (knownMafia != kNoPlayer && std::uniform_int_distribution<int>(0, 1)(rng) == 0) {
                d.kind = NightKind::Shoot;
                d.target = knownMafia;
            } else {
                d.kind = NightKind::Check;
                std::vector<PlayerId> unchecked;
                for (PlayerId id : list) {
                    if (!view.checks.contains(id)) unchecked.push_back(id);
                }
                if (unchecked.empty()) unchecked = list;
                if (unchecked.empty()) unchecked = others;
                d.target = pickFrom(unchecked, rng);
            }
            break;
        }

        case Role::Mafia:
            d.kind = NightKind::Kill;
            d.target = pickFrom(list.empty() ? others : list, rng);
            break;

        case Role::Maniac:
            d.kind = NightKind::Kill;
            d.target = pickFrom(others, rng);
            break;
    }
    return d;
}

}  // namespace mafia
