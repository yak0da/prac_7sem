#include <game/engine.hpp>

#include <stdexcept>
#include <utility>

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

void ThreadEngine::start(const std::vector<smart::SharedPtr<Player>>& players) {
    stop();
    running_ = true;
    for (const auto& player : players) {
        auto slot = smart::makeShared<Slot>();
        slot->player = player;
        const PlayerId id = player->id();
        slots_.emplace(id, slot);
        // jthread передаёт stop_token первым аргументом в callable.
        threads_.emplace_back([this, slot](std::stop_token st) { worker(*slot, std::move(st)); });
    }
}

void ThreadEngine::stop() {
    if (!running_ && threads_.empty()) return;
    for (auto& thread : threads_) thread.request_stop();
    for (auto& [_, slot] : slots_) {
        std::lock_guard lock(slot->mutex);
        slot->cv.notify_all();
    }
    threads_.clear();  // join всех jthread
    slots_.clear();
    running_ = false;
}

ThreadEngine::Slot& ThreadEngine::slotFor(PlayerId id) {
    auto it = slots_.find(id);
    if (it == slots_.end()) {
        throw std::runtime_error("ThreadEngine: нет слота для игрока " + std::to_string(id));
    }
    return *it->second;
}

void ThreadEngine::worker(Slot& slot, std::stop_token stopToken) {
    while (true) {
        RequestKind kind = RequestKind::None;
        GameView view;
        {
            std::unique_lock lock(slot.mutex);
            slot.cv.wait(lock, [&] {
                return stopToken.stop_requested() || slot.request != RequestKind::None;
            });
            if (stopToken.stop_requested() && slot.request == RequestKind::None) return;
            kind = slot.request;
            view = slot.view;
        }

        // Считаем ход вне мьютекса: ведущий ждёт ответа и не трогает этого игрока.
        Statement speakResult;
        PlayerId voteResult = kNoPlayer;
        PlayerNightDecision actResult;
        switch (kind) {
            case RequestKind::Speak: speakResult = slot.player->speak(view); break;
            case RequestKind::Vote: voteResult = slot.player->vote(view); break;
            case RequestKind::Act: actResult = slot.player->act(view); break;
            case RequestKind::None: break;
        }

        {
            std::lock_guard lock(slot.mutex);
            slot.speakResult = speakResult;
            slot.voteResult = voteResult;
            slot.actResult = actResult;
            slot.request = RequestKind::None;
            slot.responseReady = true;
        }
        slot.cv.notify_all();
    }
}

template <class Result, class SetRequest, class GetResult>
Result ThreadEngine::ask(Player& player, const GameView& view, SetRequest setRequest,
                         GetResult getResult) {
    Slot& slot = slotFor(player.id());
    {
        std::lock_guard lock(slot.mutex);
        slot.view = view;
        setRequest(slot);
        slot.responseReady = false;
    }
    slot.cv.notify_one();

    std::unique_lock lock(slot.mutex);
    slot.cv.wait(lock, [&] { return slot.responseReady; });
    return getResult(slot);
}

Statement ThreadEngine::askSpeak(Player& player, const GameView& view) {
    return ask<Statement>(
        player, view, [](Slot& s) { s.request = RequestKind::Speak; },
        [](Slot& s) { return s.speakResult; });
}

PlayerId ThreadEngine::askVote(Player& player, const GameView& view) {
    return ask<PlayerId>(
        player, view, [](Slot& s) { s.request = RequestKind::Vote; },
        [](Slot& s) { return s.voteResult; });
}

PlayerNightDecision ThreadEngine::askAct(Player& player, const GameView& view) {
    return ask<PlayerNightDecision>(
        player, view, [](Slot& s) { s.request = RequestKind::Act; },
        [](Slot& s) { return s.actResult; });
}

}  // namespace mafia
