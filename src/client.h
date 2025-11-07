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
                std::cout << "[CLIENT] PLAYER_CAT_FIRE from " << who
                    << " counts=" << count << "ts=" << timestamp << "\n";
                for (U8 c : cats) {
                    // Spawn remote projectilees
                    throw_cat((U32)c, false, -1);
                }
            }
            break;
        }
        case message_code::NEW_NOTE: {
            if (m.body.size() < sizeof(U8) + sizeof(F64) + sizeof(U8)) break;
            int index = 0;
            U8 note; U8 cannon; F64 timestamp;
            m >> note; m >> cannon; m >> timestamp;
            make_seagull(cannon, timestamp);
        }
        break;
        case message_code::SONG_START: {
            song_spb = 1;
            song_start_time = cur_time_sec;
        }
        default:
            std::cerr << "[CLIENT] Unknown message id\n";
            break;
        }
    }

    bool hello_sent_ = false;
};