#pragma once
#include <libremidi/libremidi.cpp>

std::vector<char> midi_keys_velocity(128, 0);
std::vector<char> midi_control_velocity(128, 0);
bool midi_exists = false;

void midi_callback(const libremidi::message&& message) {
	// std::cout << "Got message status: " << (int)(message[0]) << " \n";
	// how many bytes
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
		if(auto port = libremidi::midi1::in_default_port()) {
			midi.open_port(*port);
			midi_exists = true;
		}
}
