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

class servergull : public cgull::net::server_interface<message_code> {
public:
    using connection_t = cgull::net::connection<message_code>;
    explicit servergull(uint16_t port) : cgull::net::server_interface<message_code>(port) {}

    // Song control
    bool LoadSong(const std::string& midi_path, U8 lanes = 6) {
        std::string song_name;
        auto notes = midi_parse_file(midi_path, song_name);   // time in seconds, note, velocity
        if (notes.empty()) {
            std::cerr << "[SERVER] MIDI had no NOTE_ON events or failed to load.\n";
            return false;
        }

        std::lock_guard<std::mutex> lk(song_mtx_);
        lanes_ = lanes;
        schedule_.clear();
        schedule_.reserve(notes.size());
        for (const auto& n : notes) {
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
        std::lock_guard<std::mutex> lk(song_mtx_);
        if (schedule_.empty()) {
            std::cerr << "[SERVER] StartSong() called with empty schedule.\n";
            return;
        }
        song_started_ = true;
        song_start_tp_ = std::chrono::steady_clock::now();

        // Tell clients to start their local clocks now.
        cgull::net::message<message_code> m;
        m.header.id = message_code::SONG_START;
        MessageAllClients(m);

        // Broadcast the entire schedule so clients can spawn seagulls early and sync visually
        std::lock_guard<std::mutex> lk(song_mtx_);
        for (const auto& s : schedule_) {
            cgull::net::message<message_code> m;
            m.header.id = message_code::NEW_NOTE;
            m << s.rel_time;
            m << s.lane;
            m << s.note;
            MessageAllClients(m);
        }

        // Launch stupidknowitall thread (damage calculation)
        if (!stupidknowitall_thread_running_.exchange(true)) {
            stupidknowitall_thread_ = std::thread([this]() { this->StupidKnowItAllLoop(); });
        }
    }

protected:
    // server_interface overrides
    bool OnClientConnect(std::shared_ptr<connection_t> client) override {
        std::cout << "[SERVER] Client attempting connection\n";
        return true; // accept all clients
    }

    void OnClientDisconnect(std::shared_ptr<connection_t> client) override {
        std::cout << "[SERVER] Client [" << (client ? client->GetID() : 0) << "] disconnected\n";
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
                if (players_.size() < 2) {
                    players_.push_back(assigned);
                    hp_[assigned] = HEALTH_MAX;
                }
                else {
                    // spectators still get an id; hp not tracked
                    hp_.try_emplace(assigned, HEALTH_MAX);
                }
            }

            MessageClient(client, reply);
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

        case message_code::NEW_NOTE:
        case message_code::SONG_START: {
            // These arrive only from server; ignore any client sourced messages
            break;
        }

        default:
            std::cout << "[SERVER] Unknown/Unhandled message id\n";
            break;
        }
    }

private:
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
            // Find any scheduled note in this lane close enough to "now"
            // Choose the first unresolved whose |rel_time - now| <= HIT_WINDOW_SEC
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
        // Check  here an there for expired windows and apply damage
        while (stupidknowitall_thread_running_) {
            if (!song_started_) { std::this_thread::sleep_for(std::chrono::milliseconds(5)); continue; }

            const auto now_rel = SecondsSince(song_start_tp_);

            // Collect health updates
            std::optional<std::pair<U16, int>> hp0, hp1;

            {
                std::lock_guard<std::mutex> lk(song_mtx_);
                for (auto& s : schedule_) {
                    if (s.resolved) continue;
                    if (now_rel < (s.rel_time + HIT_WINDOW_SEC)) break; // schedule is sorted butttttttttttttttttttttt not ready yet

                    // Decide damage per tracked player
                    for (int p = 0; p < 2; ++p) {
                        auto pid = getPlayerId(p);
                        if (!pid.has_value()) continue;
                        // If that player did NOTT block in window, they take 1 damage and suck lmao 
                        if (!s.blocked.test(p)) {
                            auto& hp = hp_[*pid];
                            if (hp > 0) hp -= 1;
                        }
                    }
                    s.resolved = true;
                }

                // Prepare hp update payloads for up to two players
                if (players_.size() >= 1) hp0 = { players_[0], hp_[players_[0]] };
                if (players_.size() >= 2) hp1 = { players_[1], hp_[players_[1]] };
            }

            // Broadcast hp after lock
            if (hp0.has_value() || hp1.has_value()) {
                cgull::net::message<message_code> m;
                m.header.id = message_code::HEALTH_UPDATE;
                // Pack reverse: p1_id, p1_hp, p0_id, p0_hp (client reads reverse)
                if (hp1.has_value()) { m << static_cast<U16>(hp1->second); m << hp1->first; }
                else { m << static_cast<U16>(0);           m << static_cast<U16>(0xffff); }
                if (hp0.has_value()) { m << static_cast<U16>(hp0->second); m << hp0->first; }
                else { m << static_cast<U16>(0);           m << static_cast<U16>(0xffff); }
                MessageAllClients(m);
            }

            // Check game over
            {
                std::lock_guard<std::mutex> lk(players_mtx_);
                if (!game_over_sent_ && players_.size() > 0) {
                    std::optional<U16> loser;
                    for (size_t i = 0; i < players_.size() && i < 2; ++i) {
                        if (hp_[players_[i]] <= 0) loser = players_[i];
                    }
                    if (loser.has_value()) {
                        // Winner: first other tracked player with hp>0, else 0xffff
                        U16 winner = 0xffff;
                        for (size_t i = 0; i < players_.size() && i < 2; ++i) {
                            if (players_[i] != *loser && hp_[players_[i]] > 0) { winner = players_[i]; break; }
                        }
                        cgull::net::message<message_code> gm;
                        gm.header.id = message_code::GAME_OVER;
                        gm << winner;
                        MessageAllClients(gm);
                        game_over_sent_ = true;
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

    // Song schedule
    std::mutex song_mtx_;
    std::vector<ScheduledNote> schedule_;
    U8 lanes_ = 6;
    bool song_started_ = false;
    std::chrono::steady_clock::time_point song_start_tp_;

    // Stupidknowitall thread
    std::atomic<bool> stupidknowitall_thread_running_{ false };
    std::thread stupidknowitall_thread_;

    U16 NextPlayerId() {
        return ++last_player_id_;
    }
    U16 last_player_id_ = 0;
};
