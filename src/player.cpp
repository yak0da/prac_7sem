#include <game/player.hpp>

#include <algorithm>

namespace mafia {

Player::Player(PlayerId id, std::string name, Role role, smart::SharedPtr<Brain> brain,
               std::uint32_t seed)
    : brain_(std::move(brain)),
      id_(id),
      name_(std::move(name)),
      role_(role),
      rng_(seed + static_cast<std::uint32_t>(id) * 9973u) {}

int Player::trustOf(PlayerId other) const {
    auto it = trust_.find(other);
    return it == trust_.end() ? 0 : it->second;
}

void Player::addTrust(PlayerId other, int delta) {
    if (other == id_) return;
    int& value = trust_[other];
    value = std::clamp(value + delta, -100, 100);
}

void Player::rememberCheck(PlayerId id, Team seen) { checks_[id] = seen; }

PlayerNightDecision Player::act(const GameView& view) { return brain_->act(view, rng_); }

PlayerId Player::vote(const GameView& view) { return brain_->vote(view, rng_); }

Statement Player::speak(const GameView& view) { return brain_->speak(view, rng_); }

void Player::onEvent(const GameEvent& event) {
    switch (event.kind) {
        case EventKind::StatementMade:
            // Если кто-то обвинил игрока в мафии - немного меньше доверяем обвиняемому
            // и чуть больше говорящему (или наоборот при своём лагере - упрощённо).
            if (event.target != kNoPlayer && event.target != id_) {
                if (event.team == Team::Mafia) addTrust(event.target, -3);
                else addTrust(event.target, +2);
            }
            break;
        case EventKind::Executed:
        case EventKind::PlayerDied:
            if (event.target == kNoPlayer) break;
            if (event.openInfo || event.kind == EventKind::Executed) {
                // Узнали сторону/роль выбывшего: корректируем доверие к тем, кто на него голосовал -
                // здесь упрощаем: если выбывший мафия - повышаем доверие ко всем живым слегка
                // через сообщение ведущего обрабатывается в Host отдельно по голосам.
                if (teamOf(event.role) == Team::Mafia) {
                    // выбывший был мафией - наши подозрения оправдались относительно него
                    trust_.erase(event.target);
                } else if (teamOf(event.role) == Team::Town) {
                    addTrust(event.target, 0);  // уже мёртв
                }
            }
            break;
        case EventKind::NightResolved:
            break;
        case EventKind::DayStart:
        case EventKind::VoteTallied:
        case EventKind::GameEnded:
            break;
    }
}

PlayerNightDecision Civilian::act(const GameView&) {
    return PlayerNightDecision{NightKind::Skip, kNoPlayer};
}

smart::SharedPtr<Player> makePlayer(const PlayerInfo& info, smart::SharedPtr<Brain> brain,
                                    std::uint32_t seed) {
    switch (info.role) {
        case Role::Civilian:
            return smart::makeShared<Civilian>(info.id, info.name, info.role, brain, seed);
        case Role::Mafia:
            return smart::makeShared<MafiaPlayer>(info.id, info.name, info.role, brain, seed);
        case Role::Commissar:
            return smart::makeShared<Commissar>(info.id, info.name, info.role, brain, seed);
        case Role::Doctor:
            return smart::makeShared<Doctor>(info.id, info.name, info.role, brain, seed);
        case Role::Maniac:
            return smart::makeShared<Maniac>(info.id, info.name, info.role, brain, seed);
    }
    return smart::makeShared<Civilian>(info.id, info.name, info.role, brain, seed);
}

}  // namespace mafia
