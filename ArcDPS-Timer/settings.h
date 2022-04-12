#pragma once

#include "lang.h"

#include <Windows.h>
#include <string>

class Settings {
public:
	Settings(std::string file, Translation& translation);
	void save();
	void show_options();
	void show_windows();

	bool show_timer;
	bool show_segments;
	std::string server_url;
	bool auto_prepare;
	bool use_custom_id;
	std::string custom_id;
	bool disable_outside_instances;
	std::string time_formatter;
	bool hide_buttons;
	WPARAM start_key;
	WPARAM stop_key;
	WPARAM reset_key;
	WPARAM prepare_key;
	bool auto_stop;
	int early_gg_threshold;
	WPARAM segment_key;
private:
	Translation& translation;

	int settings_version;
	std::string config_file;

	char start_key_buffer[64]{};
	char stop_key_buffer[64]{};
	char reset_key_buffer[64]{};
	char prepare_key_buffer[64]{};
	char segment_key_buffer[64]{};
};
