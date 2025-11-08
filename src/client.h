#pragma once
#include <cgullnet/cgull_net.h>
#include "message.h"
#include "util.h"
#include "world_object.h"
#include <vector>
#include <cstdint>
#include <iostream>

extern U16 player_id;
extern F64 cur_time_sec;
extern std::vector<Entity> objects;
void make_seagull(U8 note, U8 cannon, F64 timestamp);

extern int  g_my_health;
extern int  g_enemy_health;
extern bool g_game_over;
extern U16  g_winner;
extern bool g_song_active;
extern bool g_sent_ready;  
extern int g_max_health;


extern U16 g_p0_id, g_p1_id;
extern bool g_p0_ready, g_p1_ready;
extern ma_engine engine;
void playSound(ma_engine* engine, const char* filePath, ma_bool32 loop, F32 pitch);

// If you’re building with C++17 or later, using inline variables here avoids ODR issues.
// Otherwise: declare these here as `extern` and define them once in a .cpp.
inline F64 song_start_time = 0.0;
inline F64 song_spb        = 0.5; // seconds/beat

static constexpr U8  SHOW_NUM_BEATS        = 10;
static constexpr F64 SEAGULL_MOVE_PER_BEAT = 5;

void throw_cat(int, bool, F64);

class seaclient : public cgull::net::client_interface<message_code> {
public:
    // Call this regularly in the main loop
    void check_messages() {
        if (!this->IsConnected()) { hello_sent_ = false; return; }

        if (!hello_sent_ && player_id == 0xffff) {
            cgull::net::message<message_code> hello;
            hello.header.id = message_code::HELLO;
            this->Send(hello);
            hello_sent_ = true;
        }

        while (this->IsConnected() && !this->Incoming().empty()) {
            try { handle_message(this->Incoming().pop_front()); }
            catch (const std::exception& e) { std::cerr << "[CLIENT] handle_message exception: " << e.what() << "\n"; }
        }

        apply_health_snapshot();
    }

    // Optional helper to send a "cat fire" event
    void send_player_cat_fire(U16 who, double timestamp, const std::vector<U8>& cats) {
        if (!this->IsConnected()) return;

        cgull::net::message<message_code> m;
        m.header.id = message_code::PLAYER_CAT_FIRE;

        // Pack in reverse: last thing you want to read goes in first
        for (int i = (int)cats.size() - 1; i >= 0; --i) m << cats[(U32)i];
        m << static_cast<U16>(cats.size());
        m << timestamp;
        m << who;

        this->Send(m);
    }

private:
    struct HealthSnapshot {
        U16 p0_id = 0xffff, p1_id = 0xffff;
        U16 p0_hp = 0, p1_hp = 0;
        bool valid = false;
    } last_health_;
    void apply_health_snapshot() {
        if (!last_health_.valid) return;
        const int def = g_max_health; // 5 in your UI
        const U16 p0hp = (last_health_.p0_id == 0xffff) ? def : last_health_.p0_hp;
        const U16 p1hp = (last_health_.p1_id == 0xffff) ? def : last_health_.p1_hp;

        if (player_id == last_health_.p0_id) { g_my_health = p0hp; g_enemy_health = p1hp; }
        else if (player_id == last_health_.p1_id) { g_my_health = p1hp; g_enemy_health = p0hp; }
        else { g_my_health = p0hp; g_enemy_health = p1hp; }
    }



    void handle_message(const cgull::net::owned_message<message_code>& owned) {
        auto m = owned.msg;

        switch (m.header.id) {
        case message_code::GIVE_PLAYER_ID: {
            if (m.body.size() < sizeof(U16)) break;
            U16 cid = 0; m >> cid; player_id = (U16)(cid & 0xffff);
            apply_health_snapshot();             
            break;
        }

        case message_code::PLAYER_CAT_FIRE: {
            // Need at least who + timestamp + count first
            if (m.body.size() < sizeof(U16) + sizeof(F64) + sizeof(U16)) break;

            U16 who = 0; F64 timestamp = 0.0; U16 count = 0;
            m >> who; m >> timestamp; m >> count;

            const size_t bytes_needed = static_cast<size_t>(count) * sizeof(U8);
            if (m.body.size() < bytes_needed) break;
            std::vector<U8> cats(count);
            for (U16 i = 0; i < count; ++i) m >> cats[i];

            // Use the provided timestamp rather than a sentinel for better sync
            if (who != player_id) {
                for (U8 c : cats) throw_cat((U32)c, false, timestamp);
            }
            break;
        }

        case message_code::NEW_NOTE: {
            if (m.body.size() < sizeof(U8) + sizeof(U8) + sizeof(F64)) break;
            U8 note = 0, cannon = 0; F64 timestamp = 0;
            m >> note; m >> cannon; m >> timestamp;
            make_seagull(note, cannon, timestamp);
            break;
        }

        case message_code::SONG_START: {
            if (m.body.size() < sizeof(F64)) break;
            m >> song_spb;
            song_start_time = cur_time_sec;
            g_song_active = true;
            g_game_over   = false;
            g_sent_ready  = false;
            std::cout << "[CLIENT] SONG_START spb=" << song_spb << "\n";
            playSound(&engine, "asset/wellerman.wav", MA_FALSE, 1.0f);
            break;
        }

        case message_code::HEALTH_UPDATE: {
            if (m.body.size() < sizeof(U16) * 4) break;

            // Pop in the order they were pushed (LIFO)
            U16 a = 0, b = 0, c = 0, d = 0;
            m >> a; m >> b; m >> c; m >> d;

            U16 p0_id = a, p0_hp = b, p1_id = c, p1_hp = d;

            // If we have lobby ids, ensure packet aligns with lobby slot ordering.
            if (g_p0_id != 0xffff && g_p1_id != 0xffff) {
                const bool looks_swapped = (p0_id == g_p1_id && p1_id == g_p0_id);
                if (looks_swapped) { std::swap(p0_id, p1_id); std::swap(p0_hp, p1_hp); }
            }

            last_health_.p0_id = p0_id; last_health_.p1_id = p1_id;
            last_health_.p0_hp = p0_hp; last_health_.p1_hp = p1_hp;
            last_health_.valid = true;

            apply_health_snapshot();
            break;
        }


        case message_code::GAME_OVER: {
            U16 winner = 0xffff; m >> winner;
            g_winner     = winner;
            g_game_over  = true;
            g_song_active= false;
            g_sent_ready = false;
            break;
        }

        case message_code::LOBBY_STATE: {
            if (m.body.size() < sizeof(U16)*2 + sizeof(U8)*2) break;
            U16 p0 = 0xffff, p1 = 0xffff; U8 r0 = 0, r1 = 0;
            m >> p0; m >> r0; m >> p1; m >> r1;
            g_p0_id = p0;  g_p1_id = p1;
            g_p0_ready = (r0 != 0); g_p1_ready = (r1 != 0);
            if (!g_song_active && !g_p0_ready && !g_p1_ready) g_sent_ready = false;
            break;
        }

        default: break;
        }
    }

    bool hello_sent_ = false;
};
