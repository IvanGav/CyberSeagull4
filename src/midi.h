#pragma once
#include <libremidi/libremidi.hpp>
#include <libremidi/reader.hpp>

std::vector<char> midi_keys_velocity(128, 0);
std::vector<char> midi_control_velocity(128, 0);
bool midi_exists = false;

void midi_callback(const libremidi::message&& message) {
	std::cout << "Got message status: " << (int)(message[0]) << " \n";
	if (message.size() == 0) {
		return;
	}
	if ((message[0] & 0b11110000) == 0b10000000) {
		// 53 55 57 59 60
		midi_keys_velocity[message[1]] = 0;
		// std::cout << "Note Off: " << (int)message[1] << " vel: " << (int)message[2] << "\n";
	}
	if ((message[0] & 0b11110000) == 0b10010000) {
		midi_keys_velocity[message[1]] = message[2];
		std::cout << "Note On: " << (int)message[1] << " vel: " << (int)message[2] << "\n";
	}
	if ((message[0] & 0b11110000) == 0b10110000) {
		std::cout << "CTRL: " << (int)message[1] << " val: " << (int)message[2] << "\n";
		midi_control_velocity[message[1]] = message[2];
	}
};


void midi_init(libremidi::midi_in& midi) {
		// Open a given midi port.
		// Alternatively, to get the default port for the system:
		std::cout << "init\n";
		if(auto port = libremidi::midi1::in_default_port()) {
			std::cout << "int2\n";
			midi.open_port(*port);
			midi_exists = true;
		}
}


typedef struct {
	U8 note;
	U8 velocity;
	F64 time;
} midi_note;


std::vector<midi_note> midi_parse_file(std::string filename, std::string& song_name) {
	std::vector<midi_note> notes;
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
