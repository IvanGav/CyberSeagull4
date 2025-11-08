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
    
    ~servergull() {
        stupidknowitall_thread_running_ = false;
        if (stupidknowitall_thread_.joinable()) stupidknowitall_thread_.join();
    }

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
        spb_ = midi.seconds_per_beat;
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
            std::scoped_lock lk(song_mtx_, players_mtx_);
            if (song_started_) return;
            if (schedule_.empty()) { std::cerr << "[SERVER] StartSong with empty schedule\n"; return; }

            for (auto& s : schedule_) { s.resolved = false; s.blocked.reset(); }

            game_over_sent_ = false;
            for (size_t i = 0; i < players_.size() && i < 2; ++i) hp_[players_[i]] = HEALTH_MAX;

            song_started_ = true;
            song_start_tp_ = std::chrono::steady_clock::now()
                + std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                    std::chrono::duration<double>(LEAD_IN_SEC));
        }

        // Tell clients to anchor clocks (no payload)
        {
            cgull::net::message<message_code> m;
            m.header.id = message_code::SONG_START;
            m << (F64)spb_;
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
                auto itp = std::find(players_.begin(), players_.end(), *pid);
                if (itp != players_.end()) {
                    players_.erase(itp);
                    ready_[0] = ready_[1] = false;
                    song_started_ = false;
                }
                hp_.erase(*pid);
            }
            /// Remove connection from deq
            auto idx = std::find(m_deqConnections.begin(), m_deqConnections.end(), client);
            if (idx != m_deqConnections.end()) {
                m_deqConnections.erase(idx);
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
            MessageAllClients(msg);

            if (msg.body.size() < sizeof(U16) + sizeof(F64) + sizeof(U16)) break;

            U16 who_wire = 0; F64 timestamp = 0.0; U16 count = 0;
            msg >> who_wire;
            msg >> timestamp;
            msg >> count;

            const size_t need = static_cast<size_t>(count) * sizeof(U8);
            if (msg.body.size() < need) break;

            std::vector<U8> cats(count);
            for (U16 i = 0; i < count; ++i) msg >> cats[i];

            {
                std::lock_guard<std::mutex> lk(song_mtx_);
                cats.erase(std::remove_if(cats.begin(), cats.end(),
                    [this](U8 lane) { return lane >= lanes_; }),
                    cats.end());
            }
            std::sort(cats.begin(), cats.end());
            cats.erase(std::unique(cats.begin(), cats.end()), cats.end());
            if (cats.empty()) break;

            std::optional<U16> pid_from_conn;
            {
                std::lock_guard<std::mutex> lk(players_mtx_);
                auto it = conn_to_pid_.find(client->GetID());
                if (it != conn_to_pid_.end()) pid_from_conn = it->second;
            }
            if (!pid_from_conn) break;

            OnPlayerFire(*pid_from_conn, cats);
            break;
        }

        case message_code::PLAYER_READY: {
            std::optional<U16> pid_from_conn;
            {
                std::lock_guard<std::mutex> lk(players_mtx_);
                auto it = conn_to_pid_.find(client->GetID());
                if (it != conn_to_pid_.end()) pid_from_conn = it->second;
            }
            if (pid_from_conn) OnPlayerReady(*pid_from_conn);
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

        int pidx = -1;
        {
            std::lock_guard<std::mutex> lk(players_mtx_);
            for (size_t i = 0; i < players_.size(); ++i)
            {
                if (players_[i] == who)
                {
                    pidx = static_cast<int>(i);
                    break;
                }
            }
            if (pidx == -1) return; // spectators no blockkk
        }

        std::lock_guard<std::mutex> lk(song_mtx_);
        for (U8 lane : cats) {
            for (auto& s : schedule_) {
                if (s.resolved || s.lane != lane) continue;
                const F64 dt = std::abs(s.rel_time - now_rel);
                if (dt <= HIT_WINDOW_SEC) s.blocked.set(pidx, true);
            }
        }
    }


    void StupidKnowItAllLoop() {
        while (stupidknowitall_thread_running_) {
            if (!song_started_) { std::this_thread::sleep_for(std::chrono::milliseconds(5)); continue; }

            const F64 now_rel = SecondsSince(song_start_tp_);

            // Locals to send
            bool send_health = false;
            bool send_game_over = false;
            U16 out_p0_id = 0xffff, out_p1_id = 0xffff;
            U16 out_p0_hp = 0, out_p1_hp = 0;
            U16 out_winner = 0xffff;


            {
                std::scoped_lock  lk(song_mtx_, players_mtx_);

                for (size_t i = 0; i < schedule_.size(); ++i) {
                    auto& s = schedule_[i];
                    if (s.resolved) continue;

                    // Because of lead in, now_rel will be negative until the count in 
                    if (now_rel < (s.rel_time + HIT_WINDOW_SEC)) break; // future notes 

                    bool b0 = s.blocked.test(0);
                    bool b1 = s.blocked.test(1);

                    auto apply_damage = [&](int idx) {
                        if (idx < 0 || idx >= (int)players_.size()) return;
                        U16 pid = players_[idx];
                        auto it = hp_.find(pid);
                        if (it != hp_.end() && it->second > 0) --(it->second); 
                    };

                    if (b0 ^ b1) {
                        // Exactly one blocked,  punish the other
                        apply_damage(b0 ? 1 : 0);
                    }
                    // else both blocked,  no damage

                    s.resolved = true;

                    for (size_t j = i + 1; j < schedule_.size(); ++j) {
                        auto& s2 = schedule_[j];
                        if (s2.resolved) continue;
                        if (s2.lane != s.lane) continue;
                        if ((s2.rel_time - s.rel_time) <= MERGE_SLICE_SEC) {
                            s2.resolved = true;
                        }
                        else break;
                    }

                }

                // Snapshot HP/IDs 
                if (players_.size() >= 1) {
                    out_p0_id = players_[0];
                    out_p0_hp = (U16)hp_[players_[0]];
                }

                if (players_.size() >= 2) {
                    out_p1_id = players_[1];
                    out_p1_hp = (U16)hp_[players_[1]];
                }

                // Send only when health/ids changed
                if (out_p0_id != last_p0_id_sent_ || out_p0_hp != last_p0_hp_sent_ ||
                    out_p1_id != last_p1_id_sent_ || out_p1_hp != last_p1_hp_sent_) {
                    send_health = true;
                    last_p0_id_sent_ = out_p0_id; last_p0_hp_sent_ = out_p0_hp;
                    last_p1_id_sent_ = out_p1_id; last_p1_hp_sent_ = out_p1_hp;
            
                }


                bool all_resolved = true;
                for (const auto& s : schedule_) {
                    if (!s.resolved) { all_resolved = false; break; }
                }

                if (!game_over_sent_ && all_resolved) {
                    // Decide winner by remaining HP; draw if equal/only one player
                    U16 winner = 0xffff;
                    if (players_.size() >= 1) {
                        int hp0 = hp_[players_[0]];
                        if (players_.size() >= 2) {
                            int hp1 = hp_[players_[1]];
                            if (hp0 > hp1) winner = players_[0];
                            else if (hp1 > hp0) winner = players_[1];
                            else                winner = 0xffff; // draw
                        }
                        else {
                            winner = players_[0]; // single player “wins”
                        }
                    }

                    out_winner = winner;
                    game_over_sent_ = true;
                    song_started_ = false;
                    ready_[0] = ready_[1] = false;
                    send_game_over = true;
                }

            }


            // HEALTH_UPDATE
            if (send_health) {
                cgull::net::message<message_code> m;
                m.header.id = message_code::HEALTH_UPDATE;
                m << out_p1_hp; m << out_p1_id; m << out_p0_hp; m << out_p0_id;
                MessageAllClients(m);
            }

            if (send_game_over) {
                cgull::net::message<message_code> gm;
                gm.header.id = message_code::GAME_OVER;
                gm << out_winner;
                MessageAllClients(gm);
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

    std::optional<U16> PidForConn(const std::shared_ptr<connection_t>& client) {
        std::lock_guard<std::mutex> lk(players_mtx_);
        auto it = conn_to_pid_.find(client->GetID());
        if (it == conn_to_pid_.end()) return std::nullopt;
        return it->second;
    }


private:
    // Player registry (first 2 tracked for hp/damage)
    std::mutex players_mtx_;
    std::unordered_map<uint32_t, U16> conn_to_pid_;
    std::vector<U16> players_;
    std::unordered_map<U16, int> hp_;
    bool game_over_sent_ = false;
    bool ready_[2] = { false,false };
    
    // last sent health snapshot
    U16 last_p0_id_sent_ = 0xffff, last_p1_id_sent_ = 0xffff;
    U16 last_p0_hp_sent_ = 0xffff, last_p1_hp_sent_ = 0xffff;

    // Song schedule
    std::mutex song_mtx_;
    std::vector<ScheduledNote> schedule_;
    U8 lanes_ = 6;
    double spb_;
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
