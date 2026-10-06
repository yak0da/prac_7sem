#include <game/engine.hpp>

#include <algorithm>
#include <ranges>
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

void CoroEngine::start(const std::vector<smart::SharedPtr<Player>>& players) {
    stop();
    running_ = true;
    for (const auto& player : players) {
        auto slot = smart::makeShared<Slot>();
        slot->player = player;
        const PlayerId id = player->id();
        // Создаём корутину; initial_suspend — сразу спит, пока не запланируем.
        slot->task = playerLoop(slot, &scheduler_);
        slots_.emplace(id, slot);
        scheduler_.schedule(slot->task.handle());
    }
    // Доводим все корутины до первого co_await WaitRequest.
    scheduler_.runUntil([this] {
        return std::ranges::all_of(slots_, [](const auto& item) { return item.second->hasWaiting; });
    });
}

void CoroEngine::stop() {
    if (!running_ && slots_.empty()) return;
    for (auto& [_, slot] : slots_) {
        slot->request = RequestKind::Stop;
        slot->responseReady = false;
        if (slot->hasWaiting) {
            slot->hasWaiting = false;
            scheduler_.schedule(slot->waiting);
        }
    }
    scheduler_.runUntil([this] {
        return std::ranges::all_of(slots_, [](const auto& item) {
            return !item.second->task.handle() || item.second->task.handle().done();
        });
    });
    for (auto& [_, slot] : slots_) slot->task.rethrowIfNeeded();
    slots_.clear();
    running_ = false;
}

CoroEngine::Slot& CoroEngine::slotFor(PlayerId id) {
    auto it = slots_.find(id);
    if (it == slots_.end()) {
        throw std::runtime_error("CoroEngine: нет слота для игрока " + std::to_string(id));
    }
    return *it->second;
}

Task CoroEngine::playerLoop(smart::SharedPtr<Slot> slot, Scheduler* scheduler) {
    while (true) {
        const RequestKind kind = co_await WaitRequest{slot.get(), scheduler};
        if (kind == RequestKind::Stop) co_return;

        switch (kind) {
            case RequestKind::Speak:
                slot->speakResult = slot->player->speak(slot->view);
                break;
            case RequestKind::Vote:
                slot->voteResult = slot->player->vote(slot->view);
                break;
            case RequestKind::Act:
                slot->actResult = slot->player->act(slot->view);
                break;
            case RequestKind::None:
            case RequestKind::Stop:
                break;
        }
        slot->request = RequestKind::None;
        slot->responseReady = true;
    }
}

template <class Result, class GetResult>
Result CoroEngine::ask(Player& player, const GameView& view, RequestKind kind, GetResult getResult) {
    Slot& slot = slotFor(player.id());
    if (!slot.hasWaiting) {
        throw std::runtime_error("CoroEngine: корутина игрока не ждёт запрос");
    }
    slot.view = view;
    slot.request = kind;
    slot.responseReady = false;
    slot.hasWaiting = false;
    scheduler_.schedule(slot.waiting);
    scheduler_.runUntil([&] { return slot.responseReady; });
    slot.task.rethrowIfNeeded();
    return getResult(slot);
}

Statement CoroEngine::askSpeak(Player& player, const GameView& view) {
    return ask<Statement>(player, view, RequestKind::Speak,
                          [](Slot& s) { return s.speakResult; });
}

PlayerId CoroEngine::askVote(Player& player, const GameView& view) {
    return ask<PlayerId>(player, view, RequestKind::Vote, [](Slot& s) { return s.voteResult; });
}

PlayerNightDecision CoroEngine::askAct(Player& player, const GameView& view) {
    return ask<PlayerNightDecision>(player, view, RequestKind::Act,
                                    [](Slot& s) { return s.actResult; });
}

}  // namespace mafia
