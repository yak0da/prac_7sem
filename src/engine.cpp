#include <game/engine.hpp>

namespace mafia {

Statement SequentialEngine::askSpeak(Player& player, const GameView& view) {
    return player.speak(view);
}

PlayerId SequentialEngine::askVote(Player& player, const GameView& view) {
    return player.vote(view);
}

PlayerNightDecision SequentialEngine::askAct(Player& player, const GameView& view) {
    return player.act(view);
}

}  // namespace mafia
