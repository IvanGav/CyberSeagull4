#pragma once
#include <cgullnet/cgull_net.h>
#include "message.h"
#include "util.h"
#include "world_object.h"
#include <vector>
#include <cstdint>

extern U16 player_id;
extern F64 cur_time_sec;
extern std::vector<Entity> objects;
void make_seagull(U8 cannon, F64 timestamp);


extern int g_my_health; 
extern int g_enemy_health;
extern bool g_game_over;
extern U16 g_winner;

// lobby state
extern U16 g_p0_id, g_p1_id;
extern bool g_p0_ready, g_p1_ready;
extern bool g_sent_ready;  

F64 song_start_time;
F64 song_spb; 
static constexpr U8 SHOW_NUM_BEATS = 4;
static constexpr F64 SEAGULL_MOVE_PER_BEAT = 30;

void throw_cat(int, bool, F64);

class seaclient : public cgull::net::client_interface<message_code> {
public:
    // Call this regularly in the main loop
    void check_messages() {
        // If not connected, make sure we will handshake next time
        if (!this->IsConnected())
        {
            hello_sent_ = false;
            return;
        };

        // If connected but dont have an id, ask server
        if (!hello_sent_ && player_id == 0xffff)
        {
            cgull::net::message<message_code> hello;
            hello.header.id = message_code::HELLO;
            this->Send(hello);
            hello_sent_ = true;
        }

        while (!this->Incoming().empty()) {
            handle_message(this->Incoming().pop_front());
        }
    }

    // Optional helper to send a "cat fire" event 
    void send_player_cat_fire(U16 who, double timestamp, const std::vector<U8>& cats) {
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
    void handle_message(const cgull::net::owned_message<message_code>& owned) {
        auto m = owned.msg;
        switch (m.header.id) {
        case message_code::GIVE_PLAYER_ID: {
            U32 cid = 0; m >> cid; player_id = (U16)(cid & 0xffff);
            break;
        }
        case message_code::PLAYER_CAT_FIRE: {
            if (m.body.size() < sizeof(U16) + sizeof(F64) + sizeof(U16)) break;
            U16 who = 0; F64 timestamp = 0.0; U16 count = 0;
            m >> who; m >> timestamp; m >> count;
            std::vector<U8> cats(count);
            for (U16 i = 0; i < count; ++i) m >> cats[i];
            if (who != player_id) for (U8 c : cats) throw_cat((U32)c, false, -1);
            break;
        }
        case message_code::NEW_NOTE: {
            if (m.body.size() < sizeof(U8) + sizeof(U8) + sizeof(F64)) break;
            U8 note = 0, cannon = 0; F64 timestamp = 0;
            m >> note; m >> cannon; m >> timestamp;
            make_seagull(cannon, timestamp);
            break;
        }
        case message_code::SONG_START: {
            song_spb = 1;                   // keep current visual pacing
            song_start_time = cur_time_sec;
            break;
        }
        case message_code::HEALTH_UPDATE: {
            // Server packs: p0_id, p0_hp, p1_id, p1_hp  (FIFO)
            U16 p0_id = 0xffff, p1_id = 0xffff, p0_hp = 0, p1_hp = 0;
            m >> p0_id; m >> p0_hp; m >> p1_id; m >> p1_hp;

            if (player_id == p0_id) { g_my_health = p0_hp; g_enemy_health = p1_hp; }
            else if (player_id == p1_id) { g_my_health = p1_hp; g_enemy_health = p0_hp; }
            else { g_my_health = p0_hp; g_enemy_health = p1_hp; } // spectator
            break;
        }
        case message_code::GAME_OVER: {
            U16 winner = 0xffff; m >> winner;
            g_winner = winner; g_game_over = true;
            g_sent_ready = false;           // allow pressing ready for next match
            break;
        }
        case message_code::LOBBY_STATE: {
            // Server packs: p0_id, p0_ready, p1_id, p1_ready
            U16 p0 = 0xffff, p1 = 0xffff; U8 r0 = 0, r1 = 0;
            m >> p0; m >> r0; m >> p1; m >> r1;
            g_p0_id = p0; g_p1_id = p1;
            g_p0_ready = (r0 != 0); g_p1_ready = (r1 != 0);
            // If a new round is forming, ensure button can be pressed again
            if (!g_p0_ready && !g_p1_ready) g_sent_ready = false;
            break;
        }
        default: break;
        }
    }

    bool hello_sent_ = false;
};