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

    struct Attempt {
        PlayerId target = kNoPlayer;
        PlayerId attacker = kNoPlayer;
        std::string cause;
        bool bodyguardCounters = true;  // false для выстрела Комиссара
        std::string witnessLabel;
    };
    std::vector<Attempt> attempts;

    if (actions.mafiaTarget != kNoPlayer && alive(players, actions.mafiaTarget)) {
        attempts.push_back({actions.mafiaTarget, actions.mafiaShooter, "убит мафией", true,
                            "покушение мафии"});
    }
    if (actions.maniacTarget != kNoPlayer && alive(players, actions.maniacTarget)) {
        // Атакующий — сам Маньяк.
        PlayerId maniacId = kNoPlayer;
        for (const auto& p : players) {
            if (p.alive && p.role == Role::Maniac) maniacId = p.id;
        }
        attempts.push_back(
            {actions.maniacTarget, maniacId, "убит маньяком", true, "покушение маньяка"});
    }
    if (actions.commissarShoots && actions.commissarTarget != kNoPlayer &&
        alive(players, actions.commissarTarget)) {
        PlayerId commissarId = kNoPlayer;
        for (const auto& p : players) {
            if (p.alive && p.role == Role::Commissar) commissarId = p.id;
        }
        attempts.push_back({actions.commissarTarget, commissarId, "убит комиссаром", false,
                            "выстрел комиссара"});
    }

    const PlayerId protectedByBg =
        (actions.bodyguardProtect != kNoPlayer && alive(players, actions.bodyguardId))
            ? actions.bodyguardProtect
            : kNoPlayer;

    std::set<PlayerId> died;
    std::set<PlayerId> counteredAttackers;

    auto markDeath = [&](PlayerId victim, const std::string& cause) {
        if (victim == kNoPlayer || !alive(players, victim)) return;
        if (died.insert(victim).second) {
            result.deaths.push_back({victim, cause});
        } else {
            auto it = std::ranges::find(result.deaths, victim, &DeathRecord::victim);
            if (it != result.deaths.end()) it->cause += " и " + cause;
        }
    };

    for (const Attempt& a : attempts) {
        // Свидетель видит покушение на подопечного (даже если спасли).
        if (actions.witnessWatch == a.target && actions.witnessId != kNoPlayer &&
            alive(players, actions.witnessId) && a.attacker != kNoPlayer) {
            result.witnessReports.push_back(
                {actions.witnessId, a.target, a.attacker, a.witnessLabel});
        }

        const bool savedByDoctor = (a.target == result.healed);
        const bool savedByBg = (a.target == protectedByBg);

        if (savedByDoctor || savedByBg) {
            if (std::ranges::find(result.savedTargets, a.target) == result.savedTargets.end()) {
                result.savedTargets.push_back(a.target);
            }
            if (savedByBg) result.bodyguardSaved = a.target;

            // Телохранитель контрудар по мафии/Маньяку; от Комиссара — только спасение.
            if (savedByBg && a.bodyguardCounters && a.attacker != kNoPlayer &&
                a.attacker != actions.bodyguardId && counteredAttackers.insert(a.attacker).second) {
                // Атакующий не должен быть спасён доктором от контрудара? Док лечит выбранную цель,
                // не атакующего — контрудар проходит.
                markDeath(a.attacker, "убит телохранителем");
                result.bodyguardKilled = a.attacker;
            }
            continue;
        }

        markDeath(a.target, a.cause);
    }

    // Проверка Комиссара (по состоянию на начало ночи). Ниндзя и Маньяк видны мирными.
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

    if (c.mafia == 0 && c.maniac == 0) return Team::Town;

    if (c.maniac == 1 && c.alive == 1) return Team::Maniac;
    if (c.maniac == 1 && c.alive == 2 && c.town == 1) return Team::Maniac;

    if (c.mafia > 0 && c.maniac == 0 && c.mafia >= (c.alive - c.mafia)) return Team::Mafia;

    return std::nullopt;
}

}  // namespace mafia
