#include "settings.h"

#include <filesystem>
#include <fstream>

#include "json.hpp"
#include "imgui/imgui.h"
#include "imgui_stdlib.h"
#include "arcdps-extension/Widgets.h"

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
	time_formatter = config.value("time_formatter", "{0:%M:%S}");
	hide_buttons = config.value("hide_buttons", false);
	auto_stop = config.value("auto_stop", false);
	early_gg_threshold = config.value("early_gg_threshold", 10);

	this->start_key = config.value("start_key", 0);
	std::string start_key = std::to_string(this->start_key);
	memset(start_key_buffer, 0, sizeof(start_key_buffer));
	start_key.copy(start_key_buffer, start_key.size());

	this->stop_key = config.value("stop_key", 0);
	std::string stop_key = std::to_string(this->stop_key);
	memset(stop_key_buffer, 0, sizeof(stop_key_buffer));
	stop_key.copy(stop_key_buffer, stop_key.size());

	this->reset_key = config.value("reset_key", 0);
	std::string reset_key = std::to_string(this->reset_key);
	memset(reset_key_buffer, 0, sizeof(reset_key_buffer));
	reset_key.copy(reset_key_buffer, reset_key.size());

	this->prepare_key = config.value("prepare_key", 0);
	std::string prepare_key = std::to_string(this->prepare_key);
	memset(prepare_key_buffer, 0, sizeof(prepare_key_buffer));
	prepare_key.copy(prepare_key_buffer, prepare_key.size());
}

void Settings::save() {
	json config;
	config["show_timer"] = show_timer;
	config["server_url"] = server_url;
	config["version"] = settings_version;
	config["sync_interval"] = sync_interval;
	config["auto_prepare"] = auto_prepare;
	config["is_offline_mode"] = is_offline_mode;
	config["disable_outside_instances"] = disable_outside_instances;
	config["timer_formatter"] = time_formatter;
	config["hide_buttons"] = hide_buttons;
	config["start_key"] = start_key;
	config["stop_key"] = stop_key;
	config["reset_key"] = reset_key;
	config["prepare_key"] = prepare_key;
	config["auto_stop"] = auto_stop;
	config["early_gg_threshold"] = early_gg_threshold;
	std::ofstream o(config_file);
	o << std::setw(4) << config << std::endl;
}

void Settings::show_options() {
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0.f, 0.f });

	ImGui::InputText("Server", &server_url);
	ImGui::InputInt("Sync Interval", &sync_interval);
	ImGui::Checkbox("Offline Mode", &is_offline_mode);
	ImGui::Separator();
	ImGui::Checkbox("Disable outside Instanced Content", &disable_outside_instances);

	ImGui::Checkbox("Auto Prepare", &auto_prepare);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Tries to automatically set the timer to prepared,\nand start on movement/skillcast. Still has a few limitations");
	}

	ImGui::Checkbox("Auto Stop", &auto_stop);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Tries to automatically stop the timer. Still experimental");
	}

	ImGui::InputInt("Early /gg threshold", &early_gg_threshold);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Seconds threshold for min duration of a boss kill, \neverything lower gets ignored for auto stop");
	}

	ImGui::Separator();
	
	ImGui::InputText("Time formatter", &time_formatter);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Format for timer time, see https://en.cppreference.com/w/cpp/chrono/duration/formatter");
	}

	ImGui::Checkbox("Hide buttons", &hide_buttons);

	ImGuiEx::KeyInput("Start Key", "##startkey", start_key_buffer, sizeof(start_key_buffer), start_key, "(not set)");
	ImGuiEx::KeyInput("Stop Key", "##stopkey", stop_key_buffer, sizeof(stop_key_buffer), stop_key, "(not set)");
	ImGuiEx::KeyInput("Reset Key", "##resetkey", reset_key_buffer, sizeof(reset_key_buffer), reset_key, "(not set)");
	ImGuiEx::KeyInput("Prepare Key", "##preparekey", prepare_key_buffer, sizeof(prepare_key_buffer), prepare_key, "(not set)");

	ImGui::PopStyleVar();
}
