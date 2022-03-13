#pragma once

#include <Windows.h>

#include <string>

class Settings {
public:
	Settings() = default;
	Settings(std::string file, int settings_version);
	void save();
	void show_options();

	bool show_timer;
	std::string server_url;
	int sync_interval;
	bool auto_prepare;
	bool is_offline_mode;
	bool disable_outside_instances;
	std::string time_formatter;
	bool hide_buttons;
	WPARAM start_key;
	WPARAM stop_key;
	WPARAM reset_key;
	WPARAM prepare_key;
	bool auto_stop;
	int early_gg_threshold;
private:
	int settings_version;
	std::string config_file;

	char start_key_buffer[64]{};
	char stop_key_buffer[64]{};
	char reset_key_buffer[64]{};
	char prepare_key_buffer[64]{};
};
