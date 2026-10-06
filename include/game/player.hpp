#pragma once

#include <smart/shared_ptr.hpp>

#include <cstdint>
#include <map>
#include <memory>
#include <random>
#include <string>

#include "brain.hpp"
#include "state.hpp"
#include "view.hpp"

namespace mafia {

// Абстрактный игрок. Конкретные роли - наследники.
class Player {
public:
    Player(PlayerId id, std::string name, Role role, smart::SharedPtr<Brain> brain, std::uint32_t seed);
    virtual ~Player() = default;

    PlayerId id() const { return id_; }
    const std::string& name() const { return name_; }
    Role role() const { return role_; }
    bool alive() const { return alive_; }
    void setAlive(bool alive) { alive_ = alive; }

    // Генератор этого игрока (seed + id) - решения не зависят от порядка потоков.
    std::mt19937& rng() { return rng_; }

    // Доверие к другим игрокам.
    int trustOf(PlayerId other) const;
    void addTrust(PlayerId other, int delta);
    const std::map<PlayerId, int>& trustMap() const { return trust_; }

    // Результаты проверок Комиссара.
    void rememberCheck(PlayerId id, Team seen);
    const std::map<PlayerId, Team>& checks() const { return checks_; }

    void setPreviousHeal(PlayerId id) { previousHeal_ = id; }
    PlayerId previousHeal() const { return previousHeal_; }

    void setLastAccused(PlayerId id) { lastAccused_ = id; }
    PlayerId lastAccused() const { return lastAccused_; }

    virtual PlayerNightDecision act(const GameView& view);
    virtual PlayerId vote(const GameView& view);
    virtual Statement speak(const GameView& view);
    virtual void onEvent(const GameEvent& event);

protected:
    smart::SharedPtr<Brain> brain_;
    std::map<PlayerId, int> trust_;
    std::map<PlayerId, Team> checks_;
    PlayerId previousHeal_ = kNoPlayer;
    PlayerId lastAccused_ = kNoPlayer;

private:
    PlayerId id_;
    std::string name_;
    Role role_;
    bool alive_ = true;
    std::mt19937 rng_;
};

class Civilian final : public Player {
public:
    using Player::Player;
    PlayerNightDecision act(const GameView& view) override;
};

class MafiaPlayer final : public Player {
public:
    using Player::Player;
};

class Commissar final : public Player {
public:
    using Player::Player;
};

class Doctor final : public Player {
public:
    using Player::Player;
};

class Maniac final : public Player {
public:
    using Player::Player;
};

// Создать игрока нужного класса по роли.
smart::SharedPtr<Player> makePlayer(const PlayerInfo& info, smart::SharedPtr<Brain> brain,
                                    std::uint32_t seed);

}  // namespace mafia
