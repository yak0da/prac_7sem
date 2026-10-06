#include <game/host.hpp>

#include <game/concepts.hpp>
#include <game/rules.hpp>
#include <game/yaml_lite.hpp>

#include <algorithm>
#include <map>
#include <random>
#include <ranges>
#include <sstream>

namespace mafia {

Host::Host(Config config, std::unique_ptr<Engine> engine, Logger logger,
           smart::SharedPtr<Brain> humanBrain)
    : config_(std::move(config)),
      engine_(std::move(engine)),
      logger_(std::move(logger)),
      humanBrainOverride_(std::move(humanBrain)) {}

GameView Host::buildView(const Player& player) const {
    GameView view;
    view.self = player.id();
    view.myRole = player.role();
    view.day = state_->day();
    view.phase = state_->phase();
    view.checks = player.checks();
    view.trust = player.trustMap();
    view.previousHeal = player.previousHeal();
    view.lastAccused = player.lastAccused();

    for (const auto& info : state_->snapshot()) {
        view.players.push_back({info.id, info.name, info.alive});
        if (isMafiaRole(player.role()) && isMafiaRole(info.role) && info.alive) {
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

void Host::applyDeath(PlayerId id, int day, const std::string& cause, Role role, bool /*announceRole*/) {
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
    event.openInfo = config_.openAnnouncements;

    std::ostringstream msg;
    msg << "Выбывает " << logger_.announcePlayer(state_->name(id), role) << " — "
        << logger_.announceCause(cause);
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
    assertRoleConcepts();

    GameOutcome outcome;
    std::mt19937 dealRng(config_.seed);
    const std::vector<Role> specials = specialRolesFor(config_);
    state_ = createGameState(config_, specials, dealRng);
    outcome.state = state_;

    FileLogger fileLog;
    fileLog.beginGame(config_);
    logger_.setFileLogger(fileLog.enabled() ? &fileLog : nullptr);
    outcome.logDirectory = fileLog.directory();

    auto botBrain = smart::makeShared<BotBrain>();
    smart::SharedPtr<Brain> humanBrain =
        humanBrainOverride_ ? humanBrainOverride_ : smart::makeShared<HumanBrain>();
    players_.clear();
    for (const auto& info : state_->snapshot()) {
        auto brain = (config_.interactive && info.human) ? humanBrain : botBrain;
        players_.push_back(makePlayer(info, brain, config_.seed));
    }
    engine_->start(players_);

    // До первого раунда — общие сведения пишем в round_00, если логи включены.
    if (fileLog.enabled()) fileLog.beginRound(0);

    logger_.info("=== Мафия ===");
    logger_.info("seed=" + std::to_string(config_.seed) + ", игроков=" +
                 std::to_string(config_.players) + ", мафии=" +
                 std::to_string(mafiaCount(config_.players, config_.mafiaDivisor)) +
                 ", объявления=" +
                 std::string(config_.openAnnouncements ? "открытые" : "закрытые") +
                 ", engine=" + engineName(config_.engine) +
                 (config_.interactive ? ", режим=интерактивный (вы — Игрок1)" : ""));
    if (fileLog.enabled()) {
        logger_.info("Файловые логи: " + fileLog.directory().string());
    }
    if (config_.fullLog) {
        logger_.detail("Полный лог: все голоса, ночные ходы, проверки, доверие.");
    }

    if (config_.interactive) {
        for (const auto& p : players_) {
            if (!p->id() || !state_->player(p->id()).human) continue;
            std::ostringstream intro;
            intro << "\n*** Лично вам ***\nВы — " << p->name() << ". Ваша роль: " << roleName(p->role())
                  << " [" << teamName(teamOf(p->role())) << "].\n";
            if (isMafiaRole(p->role())) {
                intro << "Сообщники:";
                for (const auto& other : state_->snapshot()) {
                    if (isMafiaRole(other.role) && other.id != p->id()) {
                        intro << " " << other.name;
                    }
                }
                intro << "\n";
            }
            intro << "Ведущий будет подсказывать фазу и что ввести (номер игрока).\n";
            logger_.info(intro.str());
        }
    }

    auto finish = [&](GameOutcome& out) {
        fileLog.writeSummary(out, state_->snapshot());
        out.logDirectory = fileLog.directory();
        logger_.setFileLogger(nullptr);
        engine_->stop();
        return out;
    };

    if (auto w = maybeFinish(outcome)) {
        return finish(outcome);
    }

    for (int day = 1; day <= kMaxDays; ++day) {
        state_->setDay(day);
        state_->setPhase(Phase::Day);
        outcome.daysPlayed = day;
        fileLog.beginRound(day);

        for (auto& p : players_) p->setLastAccused(kNoPlayer);

        GameEvent dayStart;
        dayStart.kind = EventKind::DayStart;
        dayStart.day = day;
        dayStart.message = "--- День " + std::to_string(day) + " ---";
        broadcast(dayStart);

        // Дневное обсуждение (через концепт PlayerLike + ranges).
        for (auto& p : players_ | std::views::filter([](const auto& pl) { return pl->alive(); })) {
            Statement s = runDaySpeak(*engine_, *p, buildView(*p));
            s.speaker = p->id();
            p->setLastAccused(s.about);

            GameEvent ev;
            ev.kind = EventKind::StatementMade;
            ev.day = day;
            ev.actor = s.speaker;
            ev.target = s.about;
            ev.team = s.claim;
            ev.message = s.text;
            broadcast(ev);
            logger_.detail(p->name() + " доверие к " + state_->name(s.about) + " = " +
                           std::to_string(p->trustOf(s.about)));
        }

        // Голосование (через концепт PlayerLike + ranges).
        logger_.info("Голосование:");
        std::vector<Vote> votes;
        for (auto& p : players_ | std::views::filter([](const auto& pl) { return pl->alive(); })) {
            PlayerId target = runDayVote(*engine_, *p, buildView(*p));
            if (target == p->id() || target == kNoPlayer || !state_->isAlive(target)) {
                auto alive = state_->aliveIds();
                std::erase(alive, p->id());
                target = alive.empty() ? kNoPlayer : alive.front();
            }
            if (target == kNoPlayer) continue;
            votes.push_back({p->id(), target});
            // В кратком логе — кто за кого; подробности доверия только в full-log.
            logger_.info("  " + p->name() + " → " + state_->name(target));
        }

        auto executed = countVotes(votes);
        {
            GameEvent tally;
            tally.kind = EventKind::VoteTallied;
            tally.day = day;
            if (executed) {
                tally.target = *executed;
                tally.message = "Итог голосования: большинство против " + state_->name(*executed);
            } else {
                tally.message = "Итог голосования: ничья — никто не выбывает.";
            }
            broadcast(tally);
        }

        if (executed) {
            Role role = state_->player(*executed).role;
            applyDeath(*executed, day, "казнён", role, true);

            for (const Vote& v : votes) {
                if (v.target != *executed) continue;
                for (auto& p : players_) {
                    if (p->id() != v.voter || !p->alive()) continue;
                    if (teamOf(role) == Team::Mafia) p->addTrust(v.target, +5);
                    else p->addTrust(v.target, -8);
                    for (auto& other : players_) {
                        if (!other->alive() || other->id() == v.voter) continue;
                        if (teamOf(role) == Team::Mafia) other->addTrust(v.voter, +4);
                        else other->addTrust(v.voter, -4);
                    }
                }
            }
        }

        if (maybeFinish(outcome)) {
            return finish(outcome);
        }

        // Ночь.
        state_->setPhase(Phase::Night);
        logger_.info("--- Ночь " + std::to_string(day) + " ---");

        NightActions actions;
        actions.previousDoctorHeal = previousDoctorHeal_;
        std::map<PlayerId, PlayerId> mafiaProposals;
        PlayerId boss = kNoPlayer;

        for (auto& p : players_) {
            if (!p->alive()) continue;
            if (isMafiaRole(p->role()) && boss == kNoPlayer) boss = p->id();

            PlayerNightDecision d = runNightTurn(*engine_, *p, buildView(*p));
            std::string targetName =
                d.target == kNoPlayer ? "—" : state_->name(d.target);
            logger_.detail(p->name() + " (" + roleName(p->role()) + "): " +
                           Logger::nightKindName(d.kind) + " → " + targetName);

            switch (p->role()) {
                case Role::Doctor:
                    if (d.kind == NightKind::Heal) actions.doctorHeal = d.target;
                    break;
                case Role::Mafia:
                case Role::Ninja:
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
                case Role::Bodyguard:
                    if (d.kind == NightKind::Protect && d.target != kNoPlayer) {
                        actions.bodyguardProtect = d.target;
                        actions.bodyguardId = p->id();
                    }
                    break;
                case Role::Witness:
                    if (d.kind == NightKind::Watch && d.target != kNoPlayer) {
                        actions.witnessWatch = d.target;
                        actions.witnessId = p->id();
                    }
                    break;
                case Role::Civilian:
                    break;
            }
        }

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
            actions.mafiaShooter = boss != kNoPlayer ? boss : mafiaProposals.begin()->first;
            logger_.detail("Мафия целится в " + state_->name(chosen) +
                           (boss != kNoPlayer && state_->isAlive(boss) ? " (решение Босса)" : " (большинство)"));
        }

        NightResult night = resolveNight(state_->snapshot(), actions);
        if (night.healRejected) outcome.doctorTriedSameHealTwice = true;

        PlayerId rememberedHeal = kNoPlayer;
        if (night.healed != kNoPlayer) {
            rememberedHeal = night.healed;
            logger_.detail("Доктор лечил " + state_->name(night.healed));
            if (config_.openAnnouncements) {
                if (!night.savedTargets.empty()) {
                    logger_.info("Утром: Доктор этой ночью спасал " + state_->name(night.healed) + ".");
                } else {
                    logger_.info("Утром: Доктор этой ночью лечил " + state_->name(night.healed) +
                                 " (покушений на него не было).");
                }
            }
        } else if (actions.doctorHeal != kNoPlayer && !night.healRejected) {
            rememberedHeal = actions.doctorHeal;
            logger_.detail("Доктор лечил " + state_->name(actions.doctorHeal));
            if (config_.openAnnouncements) {
                logger_.info("Утром: Доктор этой ночью лечил " + state_->name(actions.doctorHeal) +
                             " (покушений на него не было).");
            }
        } else if (night.healRejected) {
            logger_.detail("Доктор пытался снова лечить того же — ход отклонён.");
        }

        if (rememberedHeal != kNoPlayer) {
            previousDoctorHeal_ = rememberedHeal;
            for (auto& p : players_) {
                if (p->role() == Role::Doctor) p->setPreviousHeal(rememberedHeal);
            }
        }

        // В закрытом режиме не объясняем, почему ночь прошла без жертв.
        if (night.deaths.empty()) {
            if (config_.openAnnouncements) {
                logger_.info("Утром: этой ночью никто не погиб.");
            } else {
                logger_.info("Утро. Жертв нет.");
            }
        } else if (config_.openAnnouncements) {
            logger_.info("Утро. Итоги ночи:");
        } else {
            logger_.info("Утро. Обнаружены жертвы:");
        }

        if (night.commissarChecked != kNoPlayer && night.commissarSeenTeam) {
            for (auto& p : players_) {
                if (p->role() == Role::Commissar && p->alive()) {
                    p->rememberCheck(night.commissarChecked, *night.commissarSeenTeam);
                    logger_.detail("Комиссар проверил " + state_->name(night.commissarChecked) +
                                   ": " + teamName(*night.commissarSeenTeam));
                    if (config_.interactive && state_->player(p->id()).human) {
                        logger_.info("*** Лично вам (Комиссар) *** Проверка " +
                                     state_->name(night.commissarChecked) + ": " +
                                     teamName(*night.commissarSeenTeam) + ".");
                    }
                }
            }
        }

        for (const auto& report : night.witnessReports) {
            for (auto& p : players_) {
                if (p->id() != report.witness || !p->alive()) continue;
                p->addTrust(report.attacker, -25);
                logger_.detail("Свидетель " + p->name() + " видел: на " +
                               state_->name(report.watched) + " — " + report.action + " от " +
                               state_->name(report.attacker));
                if (config_.interactive && state_->player(p->id()).human) {
                    logger_.info("*** Лично вам (Свидетель) *** На " + state_->name(report.watched) +
                                 " этой ночью: " + report.action + ". Виновник: " +
                                 state_->name(report.attacker) + ".");
                }
            }
        }
        if (night.bodyguardKilled != kNoPlayer) {
            logger_.detail("Телохранитель устранил нападавшего " +
                           state_->name(night.bodyguardKilled));
        }

        for (const DeathRecord& death : night.deaths) {
            Role role = state_->player(death.victim).role;
            applyDeath(death.victim, day, death.cause, role, config_.openAnnouncements);
        }

        GameEvent nightEv;
        nightEv.kind = EventKind::NightResolved;
        nightEv.day = day;
        for (auto& p : players_) p->onEvent(nightEv);

        if (maybeFinish(outcome)) {
            return finish(outcome);
        }
    }

    logger_.info("Достигнут лимит дней — досрочный итог.");
    auto counts = countAliveByTeam(state_->snapshot());
    if (counts.mafia >= counts.town + counts.maniac) outcome.winner = Team::Mafia;
    else if (counts.maniac > 0 && counts.mafia == 0) outcome.winner = Team::Maniac;
    else outcome.winner = Team::Town;
    outcome.finished = true;
    outcome.daysPlayed = state_->day();
    state_->setPhase(Phase::Finished);
    return finish(outcome);
}

}  // namespace mafia
