#pragma once

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
private:
	int settings_version;
	std::string config_file;
};
