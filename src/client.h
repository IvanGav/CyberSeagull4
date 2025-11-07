#pragma once
#include <cgullnet/cgull_net.h>
#include "message.h"
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
    void send_player_cat_fire(uint16_t who, double timestamp, const std::vector<uint8_t>& cats) {
        cgull::net::message<message_code> m;
        m.header.id = message_code::PLAYER_CAT_FIRE;

        // Pack in reverse: last thing you want to read goes in first
        for (int i = (int)cats.size() - 1; i >= 0; --i) m << cats[(size_t)i];
        m << static_cast<uint16_t>(cats.size());
        m << timestamp;
        m << who;

        this->Send(m);
    }

private:
    void handle_message(const cgull::net::owned_message<message_code>& owned) {
        auto m = owned.msg;

        switch (m.header.id) {
        case message_code::GIVE_PLAYER_ID: {
            uint32_t cid = 0;
            m >> cid;
            player_id = static_cast<uint16_t>(cid & 0xffff);
            break;
        }
        case message_code::PLAYER_CAT_FIRE: {
            if (m.body.size() < sizeof(uint16_t) + sizeof(double) + sizeof(uint16_t)) break;
            uint16_t who = 0; double timestamp = 0.0; uint16_t count = 0;
            m >> who; m >> timestamp; m >> count;

            if (m.body.size() < (size_t)count) break;
            std::vector<uint8_t> cats(count);
            for (uint16_t i = 0; i < count; ++i) m >> cats[i];

            if (who != player_id) {
                std::cout << "[CLIENT] PLAYER_CAT_FIRE from " << who
                    << " counts=" << count << "ts=" << timestamp << "\n";
                for (uint8_t c : cats) {
                    // Spawn remote projectilees
                    throw_cat((int)c, false, -1);
                }
            }
            break;
        }
        case message_code::NEW_NOTE: {
            //int index = 0;
            //U8 note = msg.msg.body[index++];
            //U8 cannon = msg.msg.body[index++];
            //F64 timestamp = message_read_f64(msg.msg, index);
            //make_seagull(cannon, timestamp);
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