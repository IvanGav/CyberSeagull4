// server.h
#pragma once
#include <iostream>
#include <cstdint>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <bitset>
#include <optional>
#include <chrono>
#include "cgullnet/cgull_net.h"
#include "message.h"
#include "util.h"
#include "midi.h"

static constexpr F64 LEAD_IN_SEC = 4.0;
static constexpr F64 HIT_WINDOW_SEC = 0.25;
static constexpr F64 MERGE_SLICE_SEC = 0.06;

class servergull : public cgull::net::server_interface<message_code> {
public:
    using connection_t = cgull::net::connection<message_code>;
    explicit servergull(uint16_t port) : cgull::net::server_interface<message_code>(port) {}

    // Song control
    bool LoadSong(const std::string& midi_path, U8 lanes = 6) {
        std::string song_name;
        auto midi = midi_parse_file(midi_path, lanes);
        if (midi.notes.empty()) {
            std::cerr << "[SERVER] MIDI had no NOTE_ON events or failed to load.\n";
            return false;
        }

        std::lock_guard<std::mutex> lk(song_mtx_);
        lanes_ = lanes;
        bps_ = midi.beats_per_sec;
        schedule_.clear();
        schedule_.reserve(midi.notes.size());
        for (const auto& n : midi.notes) {
            if (n.velocity == 0) continue;
            ScheduledNote s;
            s.rel_time = n.time;
            s.note = n.note;
            s.lane = static_cast<U8>(n.note % lanes_);
            schedule_.push_back(s);
        }
        std::sort(schedule_.begin(), schedule_.end(),
            [](const ScheduledNote& a, const ScheduledNote& b)
            {
                return a.rel_time < b.rel_time;
            });
        std::cout << "[SERVER] Loaded " << schedule_.size() << " notes from '" << midi_path << "'\n";
        return true;
    }

    void StartSong() {
        {
            std::lock_guard<std::mutex> lk(song_mtx_);
            if (song_started_) return;
            if (schedule_.empty()) { std::cerr << "[SERVER] StartSong with empty schedule\n"; return; }

            // reset note state
            for (auto& s : schedule_) { s.resolved = false; s.blocked.reset(); }

            // reset match state
            {
                std::lock_guard<std::mutex> lk2(players_mtx_);
                game_over_sent_ = false;
                for (size_t i = 0; i < players_.size() && i < 2; ++i) hp_[players_[i]] = HEALTH_MAX;
            }

            song_started_ = true;

            song_start_tp_ = std::chrono::steady_clock::now()
                + std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(LEAD_IN_SEC));
        }

        // Tell clients to anchor clocks (no payload)
        {
            cgull::net::message<message_code> m;
            m.header.id = message_code::SONG_START;
            m << (F64)bps_;
            MessageAllClients(m);
        }

        {
            std::lock_guard<std::mutex> lk(song_mtx_);
            for (const auto& s : schedule_) {
                cgull::net::message<message_code> m;
                m.header.id = message_code::NEW_NOTE;

                m << (F64)(s.rel_time + LEAD_IN_SEC);
                m << (U8)s.lane;
                m << (U8)s.note;

                MessageAllClients(m);
            }
        }

        if (!stupidknowitall_thread_running_.exchange(true)) {
            stupidknowitall_thread_ = std::thread([this]() { this->StupidKnowItAllLoop(); });
        }

        std::cout << "[SERVER] SONG_START (lead-in " << LEAD_IN_SEC
            << "s), scheduled notes: " << schedule_.size() << "\n";
    }



protected:
    // server_interface overrides
    bool OnClientConnect(std::shared_ptr<connection_t> client) override {
        std::cout << "[SERVER] Client attempting connection\n";
        return true; // accept all clients
    }

    void OnClientDisconnect(std::shared_ptr<connection_t> client) override {
       std::cout << "[SERVER] Client [" << (client ? client->GetID() : 0) << "] disconnected\n";
        if (!client) return;

        std::optional<U16> pid;
        {
            std::lock_guard<std::mutex> lk(players_mtx_);
            auto it = conn_to_pid_.find(client->GetID());
            if (it != conn_to_pid_.end()) {
                pid = it->second;
                conn_to_pid_.erase(it);
            }
            if (pid.has_value()) {
                // if they were one of the two players, remove them from the slots
                auto itp = std::find(players_.begin(), players_.end(), *pid);
                if (itp != players_.end()) {
                    size_t idx = std::distance(players_.begin(), itp);
                    players_.erase(itp);
                    ready_[0] = ready_[1] = false;  
                    hp_.erase(*pid);
                    song_started_ = false;           // stop any in-progress match gating
                }
            }
        }
        SendLobbyState(); 
    }

    void OnMessage(std::shared_ptr<connection_t> client, cgull::net::message<message_code>& msg) override {
        using cgull::net::message;

        switch (msg.header.id) {
        case message_code::HELLO: {
            std::cout << "[SERVER] HELLO from client [" << (client ? client->GetID() : 0) << "]\n";
            message<message_code> reply;
            reply.header.id = message_code::GIVE_PLAYER_ID;

            U16 assigned = NextPlayerId();
            reply << assigned;
            // Record mapping and initialize health if one of first two players
            {
                std::lock_guard<std::mutex> lk(players_mtx_);
                conn_to_pid_[client->GetID()] = assigned;
              
                // first 2 become players
                if (players_.size() < 2) {
                    players_.push_back(assigned);
                    hp_[assigned] = HEALTH_MAX;
                    ready_[players_.size() - 1] = false; // not ready
                }
                else {
                    // spectators still get an id; hp not tracked
                    hp_.try_emplace(assigned, HEALTH_MAX);
                }
            }

            MessageClient(client, reply);
            SendLobbyState(); 
            break;
        }

        case message_code::PLAYER_CAT_FIRE: {
            // Relay to all clients
            MessageAllClients(msg);

            // server side collision
            U16 who = 0; F64 timestamp = 0.0; U16 count = 0;
            try {
                msg >> who; msg >> timestamp; msg >> count;
                std::vector<U8> cats(count);
                for (U16 i = 0; i < count; ++i) msg >> cats[i];

                OnPlayerFire(who, cats);
            }
            catch (...) {
                // ignore  id g af
            }
            break;
        }
        case message_code::PLAYER_READY: {
            U16 who = 0;
            if (msg.body.size() >= sizeof(U16)) {
                msg >> who;
                OnPlayerReady(who);
            }
            break;
        }

        default:
            std::cout << "[SERVER] Unknown/Unhandled message id\n";
            break;
        }
    }

private:
    // lobby and ready functions 
    void OnPlayerReady(U16 pid) {
        std::optional<int> idx = playerIndex(pid);
        if (!idx.has_value()) return; // spectators can't ready
        {
            std::lock_guard<std::mutex> lk(players_mtx_);
            ready_[*idx] = true;
        }
        SendLobbyState();
        MaybeStartMatch();
    }

    void MaybeStartMatch() {
        bool should_start = false;
        {
            std::lock_guard<std::mutex> lk(players_mtx_);
            if (!song_started_ && players_.size() >= 2 && ready_[0] && ready_[1]) {
                ready_[0] = ready_[1] = false;
                should_start = true;
            }
        }
        SendLobbyState();

        if (should_start) {
            StartSong();
        }
    }

    void SendLobbyState() {
        cgull::net::message<message_code> m;
        m.header.id = message_code::LOBBY_STATE;

        U16 p0 = 0xffff, p1 = 0xffff; U8 r0 = 0, r1 = 0;
        {
            std::lock_guard<std::mutex> lk(players_mtx_);
            if (players_.size() >= 1) { p0 = players_[0]; r0 = ready_[0] ? 1 : 0; }
            if (players_.size() >= 2) { p1 = players_[1]; r1 = ready_[1] ? 1 : 0; }
        }
        
        m << r1; m << p1; m << r0; m << p0;
        MessageAllClients(m);
    }

    // helpers
    struct ScheduledNote {
        F64  rel_time;        // seconds since SONG_START
        U8   note;
        U8   lane;
        bool resolved = false;
        std::bitset<2> blocked; // player 0/1 blocked flag
    };

    static constexpr int  HEALTH_MAX = 5;
    static constexpr F64  HIT_WINDOW_SEC = 0.25; // +/- around rel_time

    U16 NextPlayerId() {
        return ++last_player_id_;
    }

    // map a game player_id (U16) to index 0/1
    std::optional<int> playerIndex(U16 pid) {
        std::lock_guard<std::mutex> lk(players_mtx_);
        for (size_t i = 0; i < players_.size(); ++i) if (players_[i] == pid) return static_cast<int>(i);
        return std::nullopt;
    }

    void OnPlayerFire(U16 who, const std::vector<U8>& cats) {
        if (!song_started_) return;
        const auto now_rel = SecondsSince(song_start_tp_);

        auto idxOpt = playerIndex(who);
        if (!idxOpt.has_value()) return; // spectators don't block
        const int pidx = *idxOpt;

        std::lock_guard<std::mutex> lk(song_mtx_);
        for (U8 lane : cats) {
            for (auto& s : schedule_) {
                if (s.resolved) continue;
                if (s.lane != lane) continue;
                const F64 dt = std::abs(s.rel_time - now_rel);
                if (dt <= HIT_WINDOW_SEC) {
                    s.blocked.set(pidx, true);
                    // don't break,  multiple cats during window are ok
                }
            }
        }
    }

    void StupidKnowItAllLoop() {
        while (stupidknowitall_thread_running_) {
            if (!song_started_) { std::this_thread::sleep_for(std::chrono::milliseconds(5)); continue; }

            const F64 now_rel = SecondsSince(song_start_tp_);
            std::optional<std::pair<U16, int>> hp0, hp1;

            {
                std::lock_guard<std::mutex> lk(song_mtx_);

                // For each note whose window ended, resolve once
                for (size_t i = 0; i < schedule_.size(); ++i) {
                    auto& s = schedule_[i];
                    if (s.resolved) continue;

                    // Because of lead in, now_rel will be negative until the count in 
                    if (now_rel < (s.rel_time + HIT_WINDOW_SEC)) break; // future notes 

                    // Decide damage
                    bool blocked_by_any = s.blocked.any();

                    if (!blocked_by_any) {
                        for (int p = 0; p < 2; ++p) {
                            auto pid = getPlayerId(p);
                            if (!pid.has_value()) continue;
                            auto& hp = hp_[*pid];
                            if (hp > 0) hp -= 1;
                        }
                    }
                    s.resolved = true;

                    for (size_t j = i + 1; j < schedule_.size(); ++j) {
                        auto& s2 = schedule_[j];
                        if (s2.resolved) continue;
                        if (s2.lane != s.lane) continue;
                        if ((s2.rel_time - s.rel_time) <= MERGE_SLICE_SEC) {
                            s2.resolved = true;
                        }
                        else {
                            break;
                        }
                    }
                }

                if (players_.size() >= 1) hp0 = { players_[0], hp_[players_[0]] };
                if (players_.size() >= 2) hp1 = { players_[1], hp_[players_[1]] };
            }

            // HEALTH_UPDATE (client pops p0_id, p0_hp, p1_id, p1_hp → push reverse)
            if (hp0.has_value() || hp1.has_value()) {
                cgull::net::message<message_code> m;
                m.header.id = message_code::HEALTH_UPDATE;
                U16 p0_id = hp0 ? hp0->first : (U16)0xffff;
                U16 p0_hp = hp0 ? (U16)hp0->second : (U16)0;
                U16 p1_id = hp1 ? hp1->first : (U16)0xffff;
                U16 p1_hp = hp1 ? (U16)hp1->second : (U16)0;
                m << p1_hp; m << p1_id; m << p0_hp; m << p0_id;
                MessageAllClients(m);
            }

            {
                std::lock_guard<std::mutex> lk(players_mtx_);
                if (!game_over_sent_ && players_.size() > 0) {
                    std::optional<U16> loser;
                    for (size_t i = 0; i < players_.size() && i < 2; ++i) {
                        if (hp_[players_[i]] <= 0) loser = players_[i];
                    }
                    if (loser.has_value()) {
                        U16 winner = 0xffff;
                        for (size_t i = 0; i < players_.size() && i < 2; ++i)
                            if (players_[i] != *loser && hp_[players_[i]] > 0) { winner = players_[i]; break; }

                        cgull::net::message<message_code> gm;
                        gm.header.id = message_code::GAME_OVER;
                        gm << winner;
                        MessageAllClients(gm);

                        game_over_sent_ = true;
                        song_started_ = false;      // stop round
                        ready_[0] = ready_[1] = false; // require re-ready for next match
                    }
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(4));
        }
    }


    std::optional<U16> getPlayerId(int index) {
        std::lock_guard<std::mutex> lk(players_mtx_);
        if (index < 0 || index >= (int)players_.size()) return std::nullopt;
        return players_[index];
    }

    static F64 SecondsSince(const std::chrono::steady_clock::time_point& tp0) {
        using namespace std::chrono;
        return duration_cast<duration<double>>(steady_clock::now() - tp0).count();
    }

private:
    // Player registry (first 2 tracked for hp/damage)
    std::mutex players_mtx_;
    std::unordered_map<uint32_t, U16> conn_to_pid_;
    std::vector<U16> players_;
    std::unordered_map<U16, int> hp_;
    bool game_over_sent_ = false;
    bool ready_[2] = { false,false };

    // Song schedule
    std::mutex song_mtx_;
    std::vector<ScheduledNote> schedule_;
    U8 lanes_ = 6;
    double bps_;
    bool song_started_ = false;
    std::chrono::steady_clock::time_point song_start_tp_;

    // Stupidknowitall thread
    std::atomic<bool> stupidknowitall_thread_running_{ false };
    std::thread stupidknowitall_thread_;

    U16 PlayerId() {
        return ++last_player_id_;
    }
    U16 last_player_id_ = 0;
};
