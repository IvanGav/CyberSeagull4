#pragma once
#include <libremidi/libremidi.hpp>
#include <libremidi/reader.hpp>

std::vector<char> midiKeysVelocity(128, 0);
std::vector<char> midiControlVelocity(128, 0);
bool midiExists = false;

void midi_callback(const libremidi::message&& message) {
	std::cout << "Got message status: " << (int)(message[0]) << " \n";
	if (message.size() == 0) {
		return;
	}
	if ((message[0] & 0b11110000) == 0b10000000) {
		// 53 55 57 59 60
		midiKeysVelocity[message[1]] = 0;
		// std::cout << "Note Off: " << (int)message[1] << " vel: " << (int)message[2] << "\n";
	}
	if ((message[0] & 0b11110000) == 0b10010000) {
		midiKeysVelocity[message[1]] = message[2];
		playSound(&engine, "asset/cat-meow-401729-2.wav", false, noteMultiplier((U8)84, (U8)message[1]));
		std::cout << "Note On: " << (int)message[1] << " vel: " << (int)message[2] << "\n";
	}
	if ((message[0] & 0b11110000) == 0b10110000) {
		std::cout << "CTRL: " << (int)message[1] << " val: " << (int)message[2] << "\n";
		midiControlVelocity[message[1]] = message[2];
	}
};


void midi_init(libremidi::midi_in& midi) {
		// Open a given midi port.
		// Alternatively, to get the default port for the system:
		std::cout << "init\n";
		if(auto port = libremidi::midi1::in_default_port()) {
			std::cout << "int2\n";
			midi.open_port(*port);
			midiExists = true;
		}
}


typedef struct {
	U8 note;
	U8 velocity;
	F64 time;
} MIDINote;


std::vector<MIDINote> midi_parse_file(std::string filename, std::string& song_name) {
	std::vector<MIDINote> notes;
	std::ifstream file{filename, std::ios::binary};

	std::vector<uint8_t> bytes;
	bytes.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());

	libremidi::reader r;

	libremidi::reader::parse_result result = r.parse(bytes);
	F64 current_time_ms = 0;

	if(result != libremidi::reader::invalid) {
		long beat_duration = (long)(60 * 1000000. / r.startingTempo);

		for(auto& track : r.tracks) {
			for(auto& event : track) {
				current_time_ms += (event.tick / r.ticksPerBeat) * (beat_duration / 1000.f);
				if (event.m.is_meta_event())
					{
						switch (event.m.get_meta_event_type())
							{
							case libremidi::meta_event_type::TRACK_NAME:
								song_name.clear();
								for (int i = 3; i < event.m.bytes.size(); ++i)
									song_name.push_back(event.m.bytes[i]);
								break;
							case libremidi::meta_event_type::END_OF_TRACK:
							case libremidi::meta_event_type::INSTRUMENT:
							case libremidi::meta_event_type::LYRIC:
							case libremidi::meta_event_type::MARKER:
							case libremidi::meta_event_type::CUE:
							case libremidi::meta_event_type::PATCH_NAME:
							case libremidi::meta_event_type::DEVICE_NAME:
							case libremidi::meta_event_type::CHANNEL_PREFIX:
							case libremidi::meta_event_type::MIDI_PORT:
							case libremidi::meta_event_type::SEQUENCE_NUMBER:
							case libremidi::meta_event_type::TEXT:
							case libremidi::meta_event_type::COPYRIGHT:
								break;
							case libremidi::meta_event_type::TEMPO_CHANGE:
								beat_duration = ((((uint32_t)event.m.bytes[3]) << 16) + (event.m.bytes[4] << 8) + event.m.bytes[5]); // [usecs]
								// tickDuration = beatDuration / r.ticksPerBeat;
								// duration = (long)r.get_end_time() * tickDuration / 1'000'000;
								break;
							case libremidi::meta_event_type::SMPTE_OFFSET:
							case libremidi::meta_event_type::TIME_SIGNATURE:
							case libremidi::meta_event_type::KEY_SIGNATURE:
							case libremidi::meta_event_type::PROPRIETARY:
								break;
							case libremidi::meta_event_type::UNKNOWN:
								std::cout << "UNKNOWN MIDI EVENT";
								break;
							default:
								std::cout << "Unsupported.";
								break;
							}
					}
				else
					{
						switch (event.m.get_message_type())
							{
							case libremidi::message_type::NOTE_ON:
								notes.emplace_back(event.m.bytes[1], event.m.bytes[2], current_time_ms / 1000.f);
								break;
							case libremidi::message_type::NOTE_OFF:
							case libremidi::message_type::CONTROL_CHANGE:
							case libremidi::message_type::PROGRAM_CHANGE:
							case libremidi::message_type::AFTERTOUCH:
							case libremidi::message_type::POLY_PRESSURE:
							case libremidi::message_type::PITCH_BEND:
								break;
							default:
								std::cout << "Unsupported.";
								break;
							}
					}
			}
		}
	}
	file.close();

	return notes;
}

/*
// midi_track usage guide:
// I didn't want to put even more merge conflicts to main.cpp


U32 lanes = 6; // we have 6 cannons
std::string track_name;
midi_track track = midi_track::create(midi_parse_file("example.midi", track_name), lanes);

{
	...
	// game loop
	
	track.tick(dt);
	
	// while the next note needs to be played within 2 seconds,
	while(track.next_note_in() < 2.0) {
		// assuming the function `send_seagull(U8 note, U32 lane)` will put a seagull in a queue to be loaded into the cannon in exactly 2 seconds
		send_seagull(
			track.next_note().note, // note (correlated to pitch, look https://computermusicresource.com/midikeys.html)
			track.play_note() // lane
		);
	}

	// continue game loop
	...
}
*/

// at most 12 lanes supported, since currently it will not differentiate on octaves
// for mapping, go here https://computermusicresource.com/midikeys.html
struct MIDITrack {
	std::vector<MIDINote> notes;
	U32 lanes; // how many lanes we have

	F64 time; // current time from start of this track
	U32 index; // index into `notes`, where we're at
	U8 noteLaneMap[12]; // note_lane_map[n] = lane on which to send a given note

	static MIDITrack create(std::vector<MIDINote> notes, U32 lanes) {
		MIDITrack self = { notes, lanes };
		self.time = 0.0;
		self.find_note_lane_mapping();
		return self;
	}
	void tick(F64 dt) {
		time += dt;
	}
	// get the next note to be played
	MIDINote next_note() {
		return notes[index];
	}
	// time until the next note is played (if overdue, negative time)
	F64 next_note_in() {
		return notes[index].time - time;
	}
	// we played this note
	// return which lane to play it on
	// # TO GET INFO ABOUT THIS NOTE, CALL next_note BEFORE THIS FUNCTION
	U32 play_note() {
		MIDINote n = notes[index];
		index++;
		return noteLaneMap[n.note % 12];
	}
	// rewind to the beginning of this track
	void play_again() {
		time = 0.0;
		index = 0;
	}
private:
	void find_note_lane_mapping() {
		U8 notes_found[12] = {}; // mod 12
		// find used notes
		for (U32 i = 0; i < notes.size(); i++)
			notes_found[notes[i].note % 12]++;
		U8 notes_found_count = 0;
		// count how many were used
		for (U32 i = 0; i < 12; i++) notes_found_count += notes_found[i] != 0;
		if (notes_found_count == lanes) {
			// assign notes to lanes
			U8 lane_num = 0;
			for (U32 i = 0; i < 12; i++) {
				noteLaneMap[i] = lane_num;
				lane_num += notes_found != 0;
			}
		}
		else {
			// TODO fix this, it can be mapped properly
			printf("Wrong note count. Need %d, but found %d", lanes, notes_found_count);
			exit(1);
		}
	}
};
