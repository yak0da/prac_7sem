#pragma once

#include <optional>
#include <string>
#include <vector>

#include "state.hpp"
#include "types.hpp"

namespace mafia {

struct Vote {
    PlayerId voter = kNoPlayer;
    PlayerId target = kNoPlayer;
};

struct NightActions {
    PlayerId doctorHeal = kNoPlayer;
    PlayerId previousDoctorHeal = kNoPlayer;
    PlayerId mafiaTarget = kNoPlayer;
    PlayerId mafiaShooter = kNoPlayer;       // Босс / исполнитель (для контрудара Телохранителя)
    PlayerId maniacTarget = kNoPlayer;
    PlayerId commissarTarget = kNoPlayer;
    bool commissarShoots = false;
    PlayerId bodyguardProtect = kNoPlayer;   // кого защищает Телохранитель
    PlayerId bodyguardId = kNoPlayer;        // кто телохранитель
    PlayerId witnessWatch = kNoPlayer;       // за кем следит Свидетель
    PlayerId witnessId = kNoPlayer;
};

struct DeathRecord {
    PlayerId victim = kNoPlayer;
    std::string cause;
};

// Личный отчёт Свидетелю о покушении.
struct WitnessReport {
    PlayerId witness = kNoPlayer;
    PlayerId watched = kNoPlayer;
    PlayerId attacker = kNoPlayer;
    std::string action;  // "убийство мафии" / "выстрел комиссара" / ...
};

struct NightResult {
    PlayerId healed = kNoPlayer;
    bool healRejected = false;
    std::vector<DeathRecord> deaths;
    std::vector<PlayerId> savedTargets;
    PlayerId commissarChecked = kNoPlayer;
    std::optional<Team> commissarSeenTeam;
    PlayerId bodyguardSaved = kNoPlayer;
    PlayerId bodyguardKilled = kNoPlayer;  // кого контрубил Телохранитель
    std::vector<WitnessReport> witnessReports;
};

std::optional<PlayerId> countVotes(const std::vector<Vote>& votes);
NightResult resolveNight(const std::vector<PlayerInfo>& players, const NightActions& actions);
std::optional<Team> checkWinner(const std::vector<PlayerInfo>& players);

struct TeamCounts {
    int town = 0;
    int mafia = 0;
    int maniac = 0;
    int alive = 0;
};

TeamCounts countAliveByTeam(const std::vector<PlayerInfo>& players);

}  // namespace mafia
