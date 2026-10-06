#pragma once

#include <concepts>

#include "engine.hpp"
#include "player.hpp"
#include "view.hpp"

namespace mafia {

// Игрок умеет голосовать и высказываться, у него есть id.
template <class T>
concept PlayerLike = requires(T& player, const GameView& view) {
    { player.id() } -> std::convertible_to<PlayerId>;
    { player.vote(view) } -> std::same_as<PlayerId>;
    { player.speak(view) } -> std::same_as<Statement>;
};

// Игрок умеет делать ночной ход.
template <class T>
concept NightActor = PlayerLike<T> && requires(T& player, const GameView& view) {
    { player.act(view) } -> std::same_as<PlayerNightDecision>;
};

// Лекарь (Доктор): помечен константой kIsHealer.
template <class T>
concept Healer = NightActor<T> && requires {
    { T::kIsHealer } -> std::convertible_to<bool>;
} && T::kIsHealer;

// Следователь (Комиссар): помечен константой kIsInvestigator.
template <class T>
concept Investigator = NightActor<T> && requires {
    { T::kIsInvestigator } -> std::convertible_to<bool>;
} && T::kIsInvestigator;

// Шаблонные шаги хода — только для типов, удовлетворяющих концептам.
template <PlayerLike P>
Statement runDaySpeak(Engine& engine, P& player, const GameView& view) {
    return engine.askSpeak(player, view);
}

template <PlayerLike P>
PlayerId runDayVote(Engine& engine, P& player, const GameView& view) {
    return engine.askVote(player, view);
}

template <NightActor P>
PlayerNightDecision runNightTurn(Engine& engine, P& player, const GameView& view) {
    return engine.askAct(player, view);
}

// Проверка на этапе компиляции, что классы ролей удовлетворяют концептам.
inline void assertRoleConcepts() {
    static_assert(PlayerLike<Civilian>);
    static_assert(PlayerLike<MafiaPlayer>);
    static_assert(PlayerLike<Commissar>);
    static_assert(PlayerLike<Doctor>);
    static_assert(PlayerLike<Maniac>);
    static_assert(PlayerLike<Ninja>);
    static_assert(PlayerLike<Bodyguard>);
    static_assert(PlayerLike<Witness>);

    static_assert(NightActor<Civilian>);
    static_assert(NightActor<MafiaPlayer>);
    static_assert(NightActor<Commissar>);
    static_assert(NightActor<Doctor>);
    static_assert(NightActor<Maniac>);
    static_assert(NightActor<Ninja>);
    static_assert(NightActor<Bodyguard>);
    static_assert(NightActor<Witness>);

    static_assert(Healer<Doctor>);
    static_assert(Investigator<Commissar>);
    static_assert(!Healer<Civilian>);
    static_assert(!Investigator<Doctor>);
}

}  // namespace mafia
