#include <game/host.hpp>

#include <game/rules.hpp>

#include <algorithm>
#include <map>
#include <random>
#include <ranges>
#include <sstream>

namespace mafia {

Host::Host(Config config, std::unique_ptr<Engine> engine, Logger logger)
    : config_(std::move(config)), engine_(std::move(engine)), logger_(std::move(logger)) {}

GameView Host::buildView(const Player& player) const {
    GameView view;
    view.self = player.id();
    view.myRole = player.role();
    view.day = state_->day();
    view.phase = state_->phase();
    view.checks = player.checks();
    view.trust = player.trustMap();
    view.previousHeal = player.previousHeal();

    for (const auto& info : state_->snapshot()) {
        view.players.push_back({info.id, info.name, info.alive});
        if (player.role() == Role::Mafia && info.role == Role::Mafia && info.alive) {
            view.allies.push_back(info.id);
        }
    }
    return view;
}

void Host::broadcast(const GameEvent& event) {
    logger_.event(event);
    for (auto& p : players_) {
        if (p->alive() || event.kind == EventKind::GameEnded) p->onEvent(event);
    }
}

void Host::applyDeath(PlayerId id, int day, const std::string& cause, Role role, bool announceRole) {
    state_->kill(id, day, cause);
    for (auto& p : players_) {
        if (p->id() == id) p->setAlive(false);
    }

    GameEvent event;
    event.kind = EventKind::PlayerDied;
    event.day = day;
    event.target = id;
    event.role = role;
    event.team = teamOf(role);
    event.openInfo = announceRole || config_.openAnnouncements;

    std::ostringstream msg;
    msg << "Выбывает " << state_->name(id);
    if (config_.openAnnouncements) {
        msg << " (" << roleName(role) << ")";
    } else {
        msg << " [" << teamName(teamOf(role)) << "]";
    }
    msg << " — " << cause;
    event.message = msg.str();
    broadcast(event);
}

std::optional<Team> Host::maybeFinish(GameOutcome& outcome) {
    auto winner = checkWinner(state_->snapshot());
    if (!winner) return std::nullopt;

    outcome.winner = *winner;
    outcome.finished = true;
    outcome.daysPlayed = state_->day();
    state_->setPhase(Phase::Finished);

    GameEvent end;
    end.kind = EventKind::GameEnded;
    end.day = state_->day();
    end.team = *winner;
    end.message = "Игра окончена. Победили: " + teamName(*winner) + ".";
    broadcast(end);
    return winner;
}

GameOutcome Host::run() {
    GameOutcome outcome;
    std::mt19937 dealRng(config_.seed);
    state_ = createGameState(config_, defaultSpecialRoles(), dealRng);
    outcome.state = state_;

    auto brain = smart::makeShared<BotBrain>();
    players_.clear();
    for (const auto& info : state_->snapshot()) {
        // В интерактивном режиме на шаге 5 человек пока тоже играет ботом
        // (HumanBrain появится на шаге 8).
        players_.push_back(makePlayer(info, brain, config_.seed));
    }

    logger_.info("=== Мафия ===");
    logger_.info("seed=" + std::to_string(config_.seed) + ", игроков=" +
                 std::to_string(config_.players) + ", мафии=" +
                 std::to_string(mafiaCount(config_.players, config_.mafiaDivisor)) +
                 ", engine=" + engineName(config_.engine));
    logger_.detail("Роли скрыты от игроков; полный лог включён.");

    if (auto w = maybeFinish(outcome)) return outcome;

    for (int day = 1; day <= kMaxDays; ++day) {
        state_->setDay(day);
        state_->setPhase(Phase::Day);
        outcome.daysPlayed = day;

        GameEvent dayStart;
        dayStart.kind = EventKind::DayStart;
        dayStart.day = day;
        dayStart.message = "--- День " + std::to_string(day) + " ---";
        broadcast(dayStart);

        // Дневное обсуждение.
        std::vector<Statement> statements;
        for (auto& p : players_) {
            if (!p->alive()) continue;
            Statement s = engine_->askSpeak(*p, buildView(*p));
            s.speaker = p->id();
            statements.push_back(s);

            GameEvent ev;
            ev.kind = EventKind::StatementMade;
            ev.day = day;
            ev.actor = s.speaker;
            ev.target = s.about;
            ev.team = s.claim;
            ev.message = s.text;
            broadcast(ev);
            logger_.detail(p->name() + " доверие к цели " + std::to_string(p->trustOf(s.about)));
        }

        // Голосование.
        std::vector<Vote> votes;
        for (auto& p : players_) {
            if (!p->alive()) continue;
            PlayerId target = engine_->askVote(*p, buildView(*p));
            // Нельзя голосовать за мёртвого или за себя - подправляем.
            if (target == p->id() || target == kNoPlayer || !state_->isAlive(target)) {
                auto alive = state_->aliveIds();
                std::erase(alive, p->id());
                target = alive.empty() ? kNoPlayer : alive.front();
            }
            if (target == kNoPlayer) continue;
            votes.push_back({p->id(), target});
            logger_.detail(p->name() + " голосует против " + state_->name(target));
        }

        auto executed = countVotes(votes);
        {
            GameEvent tally;
            tally.kind = EventKind::VoteTallied;
            tally.day = day;
            if (executed) {
                tally.target = *executed;
                tally.message = "Голосование: большинство против " + state_->name(*executed);
            } else {
                tally.message = "Голосование: ничья — никто не выбывает.";
            }
            broadcast(tally);
        }

        if (executed) {
            Role role = state_->player(*executed).role;
            applyDeath(*executed, day, "казнён", role, true);

            // Корректировка доверия: голосовал против мафии — плюс, против мирного — минус.
            for (const Vote& v : votes) {
                if (v.target != *executed) continue;
                for (auto& p : players_) {
                    if (p->id() != v.voter || !p->alive()) continue;
                    if (teamOf(role) == Team::Mafia) p->addTrust(v.target, +5);
                    else p->addTrust(v.target, -8);
                    // Другие видят, что voter голосовал "правильно/неправильно" - упрощённо:
                    for (auto& other : players_) {
                        if (!other->alive() || other->id() == v.voter) continue;
                        if (teamOf(role) == Team::Mafia) other->addTrust(v.voter, +4);
                        else other->addTrust(v.voter, -4);
                    }
                }
            }
        }

        if (maybeFinish(outcome)) return outcome;

        // Ночь.
        state_->setPhase(Phase::Night);
        logger_.info("--- Ночь " + std::to_string(day) + " ---");

        NightActions actions;
        actions.previousDoctorHeal = previousDoctorHeal_;
        std::map<PlayerId, PlayerId> mafiaProposals;  // мафиози -> предложенная жертва
        PlayerId boss = kNoPlayer;

        for (auto& p : players_) {
            if (!p->alive()) continue;
            if (p->role() == Role::Mafia && boss == kNoPlayer) boss = p->id();

            PlayerNightDecision d = engine_->askAct(*p, buildView(*p));
            logger_.detail(p->name() + " (" + roleName(p->role()) + ") ночной ход kind=" +
                           std::to_string(static_cast<int>(d.kind)) + " target=" +
                           std::to_string(d.target));

            switch (p->role()) {
                case Role::Doctor:
                    if (d.kind == NightKind::Heal) actions.doctorHeal = d.target;
                    break;
                case Role::Mafia:
                    if (d.kind == NightKind::Kill && d.target != kNoPlayer) {
                        mafiaProposals[p->id()] = d.target;
                    }
                    break;
                case Role::Maniac:
                    if (d.kind == NightKind::Kill) actions.maniacTarget = d.target;
                    break;
                case Role::Commissar:
                    if (d.kind == NightKind::Shoot) {
                        actions.commissarShoots = true;
                        actions.commissarTarget = d.target;
                    } else if (d.kind == NightKind::Check) {
                        actions.commissarShoots = false;
                        actions.commissarTarget = d.target;
                    }
                    break;
                case Role::Civilian:
                    break;
            }
        }

        // Решение мафии: пока Босс жив — его предложение; иначе большинство.
        if (!mafiaProposals.empty()) {
            PlayerId chosen = mafiaProposals.begin()->second;
            if (boss != kNoPlayer && mafiaProposals.contains(boss)) {
                chosen = mafiaProposals[boss];
            } else {
                std::map<PlayerId, int> tallies;
                for (const auto& [_, target] : mafiaProposals) ++tallies[target];
                chosen = std::ranges::max_element(tallies, {}, &std::pair<const PlayerId, int>::second)
                             ->first;
            }
            actions.mafiaTarget = chosen;
            logger_.detail("Мафия целится в " + state_->name(chosen) +
                           (boss != kNoPlayer ? " (через Босса)" : ""));
        }

        NightResult night = resolveNight(state_->snapshot(), actions);
        if (night.healRejected) outcome.doctorTriedSameHealTwice = true;

        if (night.healed != kNoPlayer) {
            previousDoctorHeal_ = night.healed;
            for (auto& p : players_) {
                if (p->role() == Role::Doctor) p->setPreviousHeal(night.healed);
            }
            logger_.detail("Доктор лечил " + state_->name(night.healed));
            if (config_.openAnnouncements) {
                logger_.info("Доктор спас этой ночью: " + state_->name(night.healed));
            }
        } else if (actions.doctorHeal != kNoPlayer && !night.healRejected) {
            // Лечил, но в него не стреляли - всё равно запоминаем цель.
            previousDoctorHeal_ = actions.doctorHeal;
            for (auto& p : players_) {
                if (p->role() == Role::Doctor) p->setPreviousHeal(actions.doctorHeal);
            }
        }

        if (night.commissarChecked != kNoPlayer && night.commissarSeenTeam) {
            for (auto& p : players_) {
                if (p->role() == Role::Commissar && p->alive()) {
                    p->rememberCheck(night.commissarChecked, *night.commissarSeenTeam);
                    logger_.detail("Комиссар проверил " + state_->name(night.commissarChecked) +
                                   ": " + teamName(*night.commissarSeenTeam));
                }
            }
        }

        if (night.deaths.empty()) {
            logger_.info("Ночь прошла без жертв.");
        }
        for (const DeathRecord& death : night.deaths) {
            Role role = state_->player(death.victim).role;
            applyDeath(death.victim, day, death.cause, role, config_.openAnnouncements);
        }

        GameEvent nightEv;
        nightEv.kind = EventKind::NightResolved;
        nightEv.day = day;
        nightEv.message = "";
        for (auto& p : players_) p->onEvent(nightEv);

        if (maybeFinish(outcome)) return outcome;
    }

    // Защита от бесконечной игры: объявляем победу по текущему перевесу.
    logger_.info("Достигнут лимит дней — досрочный итог.");
    auto counts = countAliveByTeam(state_->snapshot());
    if (counts.mafia >= counts.town + counts.maniac) outcome.winner = Team::Mafia;
    else if (counts.maniac > 0 && counts.mafia == 0) outcome.winner = Team::Maniac;
    else outcome.winner = Team::Town;
    outcome.finished = true;
    outcome.daysPlayed = state_->day();
    state_->setPhase(Phase::Finished);
    return outcome;
}

}  // namespace mafia
