#include "settings.h"

#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "imgui/imgui.h"
#include "arcdps-extension/imgui_stdlib.h"
#include "arcdps-extension/Widgets.h"

constexpr int settings_version = 9;

Settings::Settings(std::string file, Translation& translation)
:	settings_version(settings_version),
	config_file(file),
	translation(translation)
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
	auto_prepare = config.value("auto_prepare", true);
	use_custom_id = config.value("use_custom_id", false);
	custom_id = config.value("custom_id", "");
	disable_outside_instances = config.value("disable_outside_instances", true);
	time_formatter = config.value("time_formatter", "{0:%M:%S}");
	hide_buttons = config.value("hide_buttons", false);
	auto_stop = config.value("auto_stop", false);
	early_gg_threshold = config.value("early_gg_threshold", 5);
	show_segments = config.value("show_segments", false);
	unified_window = config.value("unified_window", false);

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

	this->segment_key = config.value("segment_key", 0);
	std::string segment_key = std::to_string(this->segment_key);
	memset(segment_key_buffer, 0, sizeof(segment_key));
	prepare_key.copy(segment_key_buffer, segment_key.size());
}

void Settings::save() {
	json config;
	config["show_timer"] = show_timer;
	config["version"] = settings_version;
	config["auto_prepare"] = auto_prepare;
	config["use_custom_id"] = use_custom_id;
	config["disable_outside_instances"] = disable_outside_instances;
	config["timer_formatter"] = time_formatter;
	config["hide_buttons"] = hide_buttons;
	config["start_key"] = start_key;
	config["stop_key"] = stop_key;
	config["reset_key"] = reset_key;
	config["prepare_key"] = prepare_key;
	config["auto_stop"] = auto_stop;
	config["early_gg_threshold"] = early_gg_threshold;
	config["segment_key"] = segment_key;
	config["show_segments"] = show_segments;
	config["custom_id"] = custom_id;
	config["unified_window"] = unified_window;
	std::ofstream o(config_file);
	o << std::setw(4) << config << std::endl;
}

void Settings::show_options() {
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0.f, 0.f });

	ImGui::Checkbox(translation.get("InputUseCustomID").c_str(), &use_custom_id);
	if (use_custom_id) {
		ImGui::InputText(translation.get("InputCustomID").c_str(), &custom_id);
	}

	ImGui::Checkbox(translation.get("InputOnlyInstance").c_str(), &disable_outside_instances);

	ImGui::Checkbox(translation.get("InputAutoprepare").c_str(), &auto_prepare);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(translation.get("TooltipAutoPrepare").c_str());
	}

	ImGui::Checkbox(translation.get("InputAutostop").c_str(), &auto_stop);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(translation.get("TooltipAutoStop").c_str());
	}

	ImGui::InputInt(translation.get("InputEarlyGG").c_str(), &early_gg_threshold);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(translation.get("TooltipEarlyGG").c_str());
	}

	ImGui::Separator();
	
	ImGui::InputText(translation.get("InputTimeFormatter").c_str(), &time_formatter);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(translation.get("TooltipTimerFormatter").c_str());
	}

	ImGui::Checkbox(translation.get("InputHideButtons").c_str(), &hide_buttons);
	ImGui::Checkbox(translation.get("InputUnifiedWindow").c_str(), &unified_window);

	ImGuiEx::KeyInput(translation.get("InputStartKey").c_str(), "##startkey", start_key_buffer, sizeof(start_key_buffer), start_key, translation.get("TextKeyNotSet").c_str());
	ImGuiEx::KeyInput(translation.get("InputStopKey").c_str(), "##stopkey", stop_key_buffer, sizeof(stop_key_buffer), stop_key, translation.get("TextKeyNotSet").c_str());
	ImGuiEx::KeyInput(translation.get("InputResetKey").c_str(), "##resetkey", reset_key_buffer, sizeof(reset_key_buffer), reset_key, translation.get("TextKeyNotSet").c_str());
	ImGuiEx::KeyInput(translation.get("InputPrepareKey").c_str(), "##preparekey", prepare_key_buffer, sizeof(prepare_key_buffer), prepare_key, translation.get("TextKeyNotSet").c_str());
	ImGuiEx::KeyInput(translation.get("InputSegmentKey").c_str(), "##segmentkey", segment_key_buffer, sizeof(segment_key_buffer), segment_key, translation.get("TextKeyNotSet").c_str());

	ImGui::PopStyleVar();
}

void Settings::show_windows() {
	if (!unified_window) {
		ImGui::Checkbox(translation.get("WindowOptionTimer").c_str(), &show_timer);
		ImGui::Checkbox(translation.get("WindowOptionSegments").c_str(), &show_segments);
	}
	else {
		ImGui::Checkbox(translation.get("WindowOptionUnified").c_str(), &show_timer);
	}
}
