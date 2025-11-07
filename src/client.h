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

F64 song_start_time;
F64 song_spb = .20; // seconds/beat
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
            U32 cid = 0;
            m >> cid;
            player_id = static_cast<U16>(cid & 0xffff);
            break;
        }
        case message_code::PLAYER_CAT_FIRE: {
            if (m.body.size() < sizeof(U16) + sizeof(F64) + sizeof(U16)) break;
            U16 who = 0; F64 timestamp = 0.0; U16 count = 0;
            m >> who; m >> timestamp; m >> count;

            if (m.body.size() < (U32)count) break;
            std::vector<U8> cats(count);
            for (U16 i = 0; i < count; ++i) m >> cats[i];

            if (who != player_id) {
                for (U8 c : cats) {
                    // Spawn remote projectiles
                    throw_cat((U32)c, false, -1);
                }
            }
            break;
        }
        case message_code::NEW_NOTE: {
            if (m.body.size() < sizeof(U8) + sizeof(F64) + sizeof(U8)) break;
            U8 note, cannon; F64 timestamp;
            m >> note; m >> cannon; m >> timestamp;
            make_seagull(cannon, timestamp);
            break;
        }
        case message_code::SONG_START: {
            // Use local clock to anchor visual
            song_spb = 1;                 // adjust if you want real BPM
            song_start_time = cur_time_sec;
            break;
        }
        case message_code::HEALTH_UPDATE: {
            U16 p0_id = 0xffff; U16 p1_id = 0xffff; U16 p0_hp = 0; U16 p1_hp = 0;
            m >> p1_hp; m >> p1_id; m >> p0_hp; m >> p0_id;

            // Are we p0 or p1
            if (player_id == p0_id) {
                g_my_health = (int)p0_hp;
                g_enemy_health = (int)p1_hp;
            }
            else if (player_id == p1_id) {
                g_my_health = (int)p1_hp;
                g_enemy_health = (int)p0_hp;
            }
            else {
                // spectator: just take p0/p1 as shown
                g_my_health = (int)p0_hp;
                g_enemy_health = (int)p1_hp;
            }
            break;
        }
        case message_code::GAME_OVER: {
            U16 winner = 0xffff;
            m >> winner;
            g_winner = winner;
            g_game_over = true;
            break;
        }
        default:
            std::cerr << "[CLIENT] Unknown message id\n";
            break;
        }
    }

    bool hello_sent_ = false;
};