#pragma once

#include <smart/shared_ptr.hpp>

#include <functional>
#include <mutex>
#include <random>
#include <string>
#include <vector>

#include "config.hpp"
#include "types.hpp"

namespace mafia {

// Сведения об одном игроке, которые знает ведущий.
struct PlayerInfo {
    PlayerId id = kNoPlayer;
    std::string name;
    Role role = Role::Civilian;
    bool alive = true;
    bool human = false;      // управляется пользователем (интерактивный режим)
    int diedOnDay = 0;       // в какой день выбыл (0 - жив)
    std::string deathCause;  // как выбыл: "казнён", "убит мафией", ...
};

// Общее состояние игры. Создается один раз, хранится в SharedPtr и разделяется
// между ведущим и игроками (в том числе работающими в других потоках),
// поэтому все обращения идут под мьютексом.
class GameState {
public:
    explicit GameState(std::vector<PlayerInfo> players);

    // Копия сведений обо всех игроках (снимок на текущий момент).
    std::vector<PlayerInfo> snapshot() const;

    // Копия сведений об игроке с номером id.
    PlayerInfo player(PlayerId id) const;

    // Номера живых игроков по возрастанию.
    std::vector<PlayerId> aliveIds() const;

    bool isAlive(PlayerId id) const;
    std::string name(PlayerId id) const;
    int playerCount() const;

    int day() const;
    Phase phase() const;
    void setDay(int day);
    void setPhase(Phase phase);

    // Отметить игрока выбывшим.
    void kill(PlayerId id, int day, const std::string& cause);

    // Выполнить произвольное действие над списком игроков под мьютексом.
    void withPlayers(const std::function<void(std::vector<PlayerInfo>&)>& action);

private:
    // Доступ к игроку по номеру без блокировки (вызывающий уже держит мьютекс).
    const PlayerInfo& at(PlayerId id) const;
    PlayerInfo& at(PlayerId id);

    mutable std::mutex mutex_;
    std::vector<PlayerInfo> players_;  // players_[id - 1]
    int day_ = 0;
    Phase phase_ = Phase::Setup;
};

// Роли с особыми способностями, которые будут в игре по умолчанию.
std::vector<Role> defaultSpecialRoles();

// Количество мафии: max(1, floor(N / k)).
int mafiaCount(int players, int divisor);

// Раздача ролей: mafiaCount(N, k) мафии, затем особые роли из specials (по одной на каждую
// запись; лишние отбрасываются, если игроков не хватает), остальные - мирные жители.
// Результат перемешивается генератором rng. Элемент i - роль игрока с номером i + 1.
std::vector<Role> assignRoles(int players, int divisor, const std::vector<Role>& specials,
                              std::mt19937& rng);

// Создает состояние игры по конфигурации: раздает роли и дает игрокам имена.
// В интерактивном режиме пользователь - игрок номер 1.
smart::SharedPtr<GameState> createGameState(const Config& config, const std::vector<Role>& specials,
                                            std::mt19937& rng);

}  // namespace mafia
