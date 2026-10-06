#include <game/state.hpp>

#include <algorithm>
#include <ranges>
#include <stdexcept>
#include <utility>

namespace mafia {

GameState::GameState(std::vector<PlayerInfo> players) : players_(std::move(players)) {}

std::vector<PlayerInfo> GameState::snapshot() const {
    std::lock_guard lock(mutex_);
    return players_;
}

PlayerInfo GameState::player(PlayerId id) const {
    std::lock_guard lock(mutex_);
    return at(id);
}

std::vector<PlayerId> GameState::aliveIds() const {
    std::lock_guard lock(mutex_);
    auto alive = players_ | std::views::filter(&PlayerInfo::alive) | std::views::transform(&PlayerInfo::id);
    return {alive.begin(), alive.end()};
}

bool GameState::isAlive(PlayerId id) const {
    std::lock_guard lock(mutex_);
    return at(id).alive;
}

std::string GameState::name(PlayerId id) const {
    std::lock_guard lock(mutex_);
    return at(id).name;
}

int GameState::playerCount() const {
    std::lock_guard lock(mutex_);
    return static_cast<int>(players_.size());
}

int GameState::day() const {
    std::lock_guard lock(mutex_);
    return day_;
}

Phase GameState::phase() const {
    std::lock_guard lock(mutex_);
    return phase_;
}

void GameState::setDay(int day) {
    std::lock_guard lock(mutex_);
    day_ = day;
}

void GameState::setPhase(Phase phase) {
    std::lock_guard lock(mutex_);
    phase_ = phase;
}

void GameState::kill(PlayerId id, int day, const std::string& cause) {
    std::lock_guard lock(mutex_);
    PlayerInfo& info = at(id);
    info.alive = false;
    info.diedOnDay = day;
    info.deathCause = cause;
}

void GameState::withPlayers(const std::function<void(std::vector<PlayerInfo>&)>& action) {
    std::lock_guard lock(mutex_);
    action(players_);
}

const PlayerInfo& GameState::at(PlayerId id) const {
    if (id < 1 || id > static_cast<int>(players_.size())) {
        throw std::out_of_range("нет игрока с номером " + std::to_string(id));
    }
    return players_[static_cast<std::size_t>(id - 1)];
}

PlayerInfo& GameState::at(PlayerId id) {
    return const_cast<PlayerInfo&>(std::as_const(*this).at(id));
}

std::vector<Role> defaultSpecialRoles() {
    return {Role::Commissar, Role::Doctor, Role::Maniac};
}

int mafiaCount(int players, int divisor) {
    return std::max(1, players / divisor);
}

std::vector<Role> assignRoles(int players, int divisor, const std::vector<Role>& specials,
                              std::mt19937& rng) {
    int mafia = mafiaCount(players, divisor);
    std::vector<Role> roles(static_cast<std::size_t>(mafia), Role::Mafia);
    for (Role special : specials) {
        if (static_cast<int>(roles.size()) == players) break;
        roles.push_back(special);
    }
    roles.resize(static_cast<std::size_t>(players), Role::Civilian);
    std::ranges::shuffle(roles, rng);
    return roles;
}

smart::SharedPtr<GameState> createGameState(const Config& config, const std::vector<Role>& specials,
                                            std::mt19937& rng) {
    auto roles = assignRoles(config.players, config.mafiaDivisor, specials, rng);
    std::vector<PlayerInfo> players;
    for (int i = 0; i < config.players; ++i) {
        PlayerInfo info;
        info.id = i + 1;
        info.name = "Игрок" + std::to_string(i + 1);
        info.role = roles[static_cast<std::size_t>(i)];
        info.human = config.interactive && info.id == 1;
        players.push_back(std::move(info));
    }
    return smart::makeShared<GameState>(std::move(players));
}

}  // namespace mafia
