#include "settings.h"

#include <filesystem>
#include <fstream>

#include "json.hpp"

using json = nlohmann::json;

Settings::Settings(std::string file, int settings_version)
:	settings_version(settings_version),
	config_file(file)
{
	json config;
	config["filler"] = "empty";
	if (std::filesystem::exists(config_file)) {
		std::ifstream input(config_file);
		input >> config;
		if (config["version"] < settings_version) {
			config.clear();
		}
	}
	show_timer = config.value("show_timer", true);
	server_url = config.value("server_url", "http://164.92.229.177:5001/");
	sync_interval = config.value("sync_interval", 1);
	auto_prepare = config.value("auto_prepare", true);
	is_offline_mode = config.value("is_offline_mode", false);
	disable_outside_instances = config.value("disable_outside_instances", true);
}

void Settings::save()
{
	json config;
	config["show_timer"] = show_timer;
	config["server_url"] = server_url;
	config["version"] = settings_version;
	config["sync_interval"] = sync_interval;
	config["auto_prepare"] = auto_prepare;
	config["is_offline_mode"] = is_offline_mode;
	config["disable_outside_instances"] = disable_outside_instances;
	std::ofstream o(config_file);
	o << std::setw(4) << config << std::endl;
}
