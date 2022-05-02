#include "settings.h"

#include <filesystem>
#include <fstream>

using json = nlohmann::json;

#include "arcdps.h"
#include "arcdps-extension/imgui_stdlib.h"
#include "arcdps-extension/Widgets.h"
#include "arcdps-extension/KeyInput.h"

constexpr int current_settings_version = 10;

Settings::Settings(std::string file, Translation& translation)
:	settings_version(current_settings_version),
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
	save_logs = config.value("save_logs", true);
	start_key = config.value("start_key", KeyBinds::Key());
	start_key = config.value("stop_key", KeyBinds::Key());
	start_key = config.value("reset_key", KeyBinds::Key());
	start_key = config.value("prepare_key", KeyBinds::Key());
	start_key = config.value("segment_key", KeyBinds::Key());

	ImVec4 default_color = ImVec4(0.62f, 0.60f, 0.65f, 0.30f);
	start_button_color = config.value("start_button_color", default_color);
	stop_button_color = config.value("stop_button_color", default_color);
	reset_button_color = config.value("reset_button_color", default_color);
	prepare_button_color = config.value("prepare_button_color", default_color);
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
	config["start_button_color"] = start_button_color;
	config["stop_button_color"] = stop_button_color;
	config["reset_button_color"] = reset_button_color;
	config["prepare_button_color"] = prepare_button_color;
	config["save_logs"] = save_logs;
	std::ofstream o(config_file);
	o << std::setw(4) << config << std::endl;
}

void Settings::mod_options() {
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0.f, 0.f });

	ImGui::Checkbox(translation.get("InputSaveLogs").c_str(), &save_logs);

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

	ImGuiEx::KeyCodeInput(translation.get("InputStartKey").c_str(), start_key, Language::English, current_hkl);
	ImGuiEx::KeyCodeInput(translation.get("InputStopKey").c_str(), stop_key, Language::English, current_hkl);
	ImGuiEx::KeyCodeInput(translation.get("InputResetKey").c_str(), reset_key, Language::English, current_hkl);
	ImGuiEx::KeyCodeInput(translation.get("InputPrepareKey").c_str(), prepare_key, Language::English, current_hkl);
	ImGuiEx::KeyCodeInput(translation.get("InputSegmentKey").c_str(), segment_key, Language::English, current_hkl);

	ImGui::Separator();

	if (ImGui::ColorButton(translation.get("InputStartButtonColor").c_str(), start_button_color)) {
		ImGui::OpenPopup("##popupstartcolour");
	}
	ImGui::SameLine();
	ImGui::LabelText("##startbuttoncolor", translation.get("InputStartButtonColor").c_str());

	if (ImGui::ColorButton(translation.get("InputStopButtonColor").c_str(), stop_button_color)) {
		ImGui::OpenPopup("##popupstopcolour");
	}
	ImGui::SameLine();
	ImGui::LabelText("##stopbuttoncolor", translation.get("InputStopButtonColor").c_str());

	if (ImGui::ColorButton(translation.get("InputResetButtonColor").c_str(), reset_button_color)) {
		ImGui::OpenPopup("##popupresetcolour");
	}
	ImGui::SameLine();
	ImGui::LabelText("##resetbuttoncolor", translation.get("InputResetButtonColor").c_str());

	if (ImGui::ColorButton(translation.get("InputPrepareButtonColor").c_str(), prepare_button_color)) {
		ImGui::OpenPopup("##popuppreparecolour");
	}
	ImGui::SameLine();
	ImGui::LabelText("##preparebuttoncolor", translation.get("InputPrepareButtonColor").c_str());

	ImGui::PopStyleVar();

	if (ImGui::BeginPopup("##popupstartcolour")) {
		float color[4] = { start_button_color.x, start_button_color.y, start_button_color.z, start_button_color.w };
		ImGui::ColorPicker4(translation.get("InputStartButtonColor").c_str(), color);
		start_button_color = ImVec4(color[0], color[1], color[2], color[3]);

		ImGui::EndPopup();
	}

	if (ImGui::BeginPopup("##popupstopcolour")) {
		float color[4] = { stop_button_color.x, stop_button_color.y, stop_button_color.z, stop_button_color.w };
		ImGui::ColorPicker4(translation.get("InputStopButtonColor").c_str(), color);
		stop_button_color = ImVec4(color[0], color[1], color[2], color[3]);

		ImGui::EndPopup();
	}

	if (ImGui::BeginPopup("##popupresetcolour")) {
		float color[4] = { reset_button_color.x, reset_button_color.y, reset_button_color.z, reset_button_color.w };
		ImGui::ColorPicker4(translation.get("InputResetButtonColor").c_str(), color);
		reset_button_color = ImVec4(color[0], color[1], color[2], color[3]);

		ImGui::EndPopup();
	}

	if (ImGui::BeginPopup("##popuppreparecolour")) {
		float color[4] = { prepare_button_color.x, prepare_button_color.y, prepare_button_color.z, prepare_button_color.w };
		ImGui::ColorPicker4(translation.get("InputPrepareButtonColor").c_str(), color);
		prepare_button_color = ImVec4(color[0], color[1], color[2], color[3]);

		ImGui::EndPopup();
	}
}

void Settings::mod_windows() {
	if (!unified_window) {
		ImGui::Checkbox(translation.get("WindowOptionTimer").c_str(), &show_timer);
		ImGui::Checkbox(translation.get("WindowOptionSegments").c_str(), &show_segments);
	}
	else {
		ImGui::Checkbox(translation.get("WindowOptionUnified").c_str(), &show_timer);
	}
}

bool Settings::mod_wnd(HWND pWindowHandle, UINT pMessage, WPARAM pAdditionalW, LPARAM pAdditionalL) {
	return ImGuiEx::KeyCodeInputWndHandle(pWindowHandle, pMessage, pAdditionalW, pAdditionalL);
}
