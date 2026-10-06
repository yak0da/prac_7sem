#include <game/rules.hpp>

#include <algorithm>
#include <map>
#include <ranges>
#include <set>

namespace mafia {

namespace {

const PlayerInfo* findPlayer(const std::vector<PlayerInfo>& players, PlayerId id) {
    auto it = std::ranges::find(players, id, &PlayerInfo::id);
    return it == players.end() ? nullptr : &*it;
}

bool alive(const std::vector<PlayerInfo>& players, PlayerId id) {
    const PlayerInfo* p = findPlayer(players, id);
    return p && p->alive;
}

}  // namespace

std::optional<PlayerId> countVotes(const std::vector<Vote>& votes) {
    // Считаем только голоса с валидной целью.
    auto valid = votes | std::views::filter([](const Vote& v) { return v.target != kNoPlayer; });

    std::map<PlayerId, int> tallies;
    for (const Vote& v : valid) ++tallies[v.target];
    if (tallies.empty()) return std::nullopt;

    const int maxVotes = std::ranges::max(tallies | std::views::values);
    auto leadersView = tallies | std::views::filter([maxVotes](const auto& entry) {
                           return entry.second == maxVotes;
                       }) |
                       std::views::keys;
    std::vector<PlayerId> leaders{leadersView.begin(), leadersView.end()};

    // Ничья: никто не выбывает.
    if (leaders.size() != 1) return std::nullopt;
    return leaders.front();
}

TeamCounts countAliveByTeam(const std::vector<PlayerInfo>& players) {
    TeamCounts counts;
    for (const PlayerInfo& p : players | std::views::filter(&PlayerInfo::alive)) {
        ++counts.alive;
        switch (teamOf(p.role)) {
            case Team::Town: ++counts.town; break;
            case Team::Mafia: ++counts.mafia; break;
            case Team::Maniac: ++counts.maniac; break;
        }
    }
    return counts;
}

NightResult resolveNight(const std::vector<PlayerInfo>& players, const NightActions& actions) {
    NightResult result;

    // Лечение: нельзя два раза подряд одного и того же; мёртвого лечить бессмысленно.
    PlayerId healTarget = actions.doctorHeal;
    if (healTarget != kNoPlayer) {
        if (healTarget == actions.previousDoctorHeal) {
            result.healRejected = true;
            healTarget = kNoPlayer;
        } else if (!alive(players, healTarget)) {
            healTarget = kNoPlayer;
        } else {
            result.healed = healTarget;
        }
    }

    // Попытки убийства: цель -> причина. Если в одного стреляют несколько -
    // сохраняем все причины, но смерть одна (если не спасён).
    struct Attempt {
        PlayerId target;
        std::string cause;
    };
    std::vector<Attempt> attempts;

    if (actions.mafiaTarget != kNoPlayer && alive(players, actions.mafiaTarget)) {
        attempts.push_back({actions.mafiaTarget, "убит мафией"});
    }
    if (actions.maniacTarget != kNoPlayer && alive(players, actions.maniacTarget)) {
        attempts.push_back({actions.maniacTarget, "убит маньяком"});
    }
    if (actions.commissarShoots && actions.commissarTarget != kNoPlayer &&
        alive(players, actions.commissarTarget)) {
        attempts.push_back({actions.commissarTarget, "убит комиссаром"});
    }

    // Доктор спасает ото всех покушений на подопечного (мафия, Маньяк, Комиссар).
    std::set<PlayerId> died;
    for (const Attempt& a : attempts) {
        if (a.target == result.healed) {
            if (std::ranges::find(result.savedTargets, a.target) == result.savedTargets.end()) {
                result.savedTargets.push_back(a.target);
            }
            continue;
        }
        if (died.insert(a.target).second) {
            result.deaths.push_back({a.target, a.cause});
        } else {
            // Уже убит этой ночью другим - дописываем причину в запись.
            auto it = std::ranges::find(result.deaths, a.target, &DeathRecord::victim);
            if (it != result.deaths.end()) it->cause += " и " + a.cause;
        }
    }

    // Проверка Комиссара (если не стрелял). Маньяк виден как мирный.
    if (!actions.commissarShoots && actions.commissarTarget != kNoPlayer) {
        if (const PlayerInfo* target = findPlayer(players, actions.commissarTarget);
            target && target->alive) {
            result.commissarChecked = actions.commissarTarget;
            result.commissarSeenTeam = seenByCommissar(target->role);
        }
    }

    return result;
}

std::optional<Team> checkWinner(const std::vector<PlayerInfo>& players) {
    const TeamCounts c = countAliveByTeam(players);
    if (c.alive == 0) return Team::Town;

    // Мирные: нет ни мафии, ни Маньяка.
    if (c.mafia == 0 && c.maniac == 0) return Team::Town;

    // Маньяк: один на один с мирным, либо остался один.
    if (c.maniac == 1 && c.alive == 1) return Team::Maniac;
    if (c.maniac == 1 && c.alive == 2 && c.town == 1) return Team::Maniac;

    // Мафия: численное преимущество или равенство с остальными,
    // но если среди оставшихся жив Маньяк - игра продолжается.
    if (c.mafia > 0 && c.maniac == 0 && c.mafia >= (c.alive - c.mafia)) return Team::Mafia;

    return std::nullopt;
}

}  // namespace mafia
