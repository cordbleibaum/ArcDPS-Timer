#include "settings.h"

#include <filesystem>
#include <fstream>
#include <shellapi.h>

using json = nlohmann::json;

#include "arcdps.h"
#include "arcdps-extension/imgui_stdlib.h"
#include "arcdps-extension/Widgets.h"
#include "arcdps-extension/KeyInput.h"

constexpr int current_settings_version = 10;

Settings::Settings(std::string file, const Translation& translation, KeyBindHandler& keybind_handler, const MapTracker& map_tracker, GW2MumbleLink& mumble_link)
:	settings_version(current_settings_version),
	config_file(file),
	translation(translation),
	keybind_handler(keybind_handler),
	map_tracker(map_tracker),
	mumble_link(mumble_link)
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
	segment_time_formatter = config.value("segment_time_formatter", "{0:%M:%S}");
	hide_timer_buttons = config.value("hide_timer_buttons", false);
	hide_segment_buttons = config.value("hide_segment_buttons", false);
	segment_window_border = config.value("segment_window_border", true);
	auto_stop = config.value("auto_stop", false);
	show_segments = config.value("show_segments", false);
	unified_window = config.value("unified_window", false);
	save_logs = config.value("save_logs", true);
	start_key = config.value("start_key", KeyBinds::Key());
	stop_key = config.value("stop_key", KeyBinds::Key());
	reset_key = config.value("reset_key", KeyBinds::Key());
	prepare_key = config.value("prepare_key", KeyBinds::Key());
	segment_key = config.value("segment_key", KeyBinds::Key());
	additional_boss_ids = config.value("additional_boss_ids", std::set<int>());
	disable_in_fractal_lobby = config.value("disable_in_fractal_lobby", true);

	const ImVec4 default_color = ImVec4(0.62f, 0.60f, 0.65f, 0.30f);
	start_button_color = config.value("start_button_color", default_color);
	stop_button_color = config.value("stop_button_color", default_color);
	reset_button_color = config.value("reset_button_color", default_color);
	prepare_button_color = config.value("prepare_button_color", default_color);
	segment_button_color = config.value("segment_button_color", default_color);
	clear_button_color = config.value("clear_button_color", default_color);

	SettingsSet fractal_defaults;
	fractal_defaults.auto_prepare = true;
	dungeon_fractal_settings = config.value("dungeon_fractal_settings", fractal_defaults);
	raid_settings = config.value("raid_settings", SettingsSet());
	strike_settings = config.value("strike_settings", SettingsSet());
}

void Settings::mod_release() {
	json config;
	config["show_timer"] = show_timer;
	config["version"] = settings_version;
	config["auto_prepare"] = auto_prepare;
	config["use_custom_id"] = use_custom_id;
	config["settings"] = disable_outside_instances;
	config["timer_formatter"] = time_formatter;
	config["hide_timer_buttons"] = hide_timer_buttons;
	config["hide_segment_buttons"] = hide_segment_buttons;
	config["start_key"] = start_key;
	config["stop_key"] = stop_key;
	config["reset_key"] = reset_key;
	config["prepare_key"] = prepare_key;
	config["auto_stop"] = auto_stop;
	config["segment_key"] = segment_key;
	config["show_segments"] = show_segments;
	config["custom_id"] = custom_id;
	config["unified_window"] = unified_window;
	config["start_button_color"] = start_button_color;
	config["stop_button_color"] = stop_button_color;
	config["reset_button_color"] = reset_button_color;
	config["prepare_button_color"] = prepare_button_color;
	config["segment_button_color"] = segment_button_color;
	config["clear_button_color"] = clear_button_color;
	config["save_logs"] = save_logs;
	config["additional_boss_ids"] = additional_boss_ids;
	config["segment_time_formatter"] = segment_time_formatter;
	config["segment_window_border"] = segment_window_border;
	config["disable_in_fractal_lobby"] = disable_in_fractal_lobby;
	config["dungeon_fractal_settings"] = dungeon_fractal_settings;
	config["raid_settings"] = raid_settings;
	config["strike_settings"] = strike_settings;
	std::ofstream o(config_file);
	o << std::setw(4) << config << std::endl;
}

void Settings::mod_options() {	
	ImGui::BeginTabBar("#tabs_settings");

	if (ImGui::BeginTabItem(translation.get("TabGeneral").c_str())) {
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0.f, 0.f });

		ImGui::Checkbox(translation.get("InputSaveLogs").c_str(), &save_logs);

		ImGui::Checkbox(translation.get("InputUseCustomID").c_str(), &use_custom_id);
		if (use_custom_id) {
			ImGui::InputText(translation.get("InputCustomID").c_str(), &custom_id);
		}

		ImGui::Checkbox(translation.get("InputOnlyInstance").c_str(), &disable_outside_instances);
		if (disable_outside_instances) {
			ImGui::Checkbox(translation.get("InputDisableFractalLobby").c_str(), &disable_in_fractal_lobby);
		}

		ImGui::Checkbox(translation.get("InputAutoprepare").c_str(), &auto_prepare);
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(translation.get("TooltipAutoPrepare").c_str());
		}

		ImGui::Checkbox(translation.get("InputAutostop").c_str(), &auto_stop);
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(translation.get("TooltipAutoStop").c_str());
		}

		ImGui::Separator();

		ImGui::Text(translation.get("TextAdditionalBossIDs").c_str());

		ImGui::BeginTable("##bossidstable", 1);
		for (int id : additional_boss_ids) {
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			if (ImGui::Selectable(std::to_string(id).c_str(), id == boss_id_selected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap)) {
				boss_id_selected = id;
			}
		}
		ImGui::EndTable();

		ImGui::InputInt(translation.get("InputBossID").c_str(), &boss_id_input);
		if (ImGui::Button(translation.get("ButtonAdd").c_str())) {
			additional_boss_ids.insert(boss_id_input);
		}
		ImGui::SameLine();
		ImGui::PushID("delete_set");
		if (ImGui::Button(translation.get("ButtonRemove").c_str())) {
			additional_boss_ids.erase(boss_id_selected);
		}
		ImGui::PopID();

		ImGui::Separator();

		ImGui::InputText(translation.get("InputTimeFormatter").c_str(), &time_formatter);
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(translation.get("TooltipTimerFormatter").c_str());
		}

		ImGui::InputText(translation.get("InputSegmentTimeFormatter").c_str(), &segment_time_formatter);
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(translation.get("TooltipTimerFormatter").c_str());
		}

		if (ImGuiEx::KeyCodeInput(translation.get("InputStartKey").c_str(), start_key, Language::English, current_hkl)) {
			keybind_handler.UpdateKey(start_key_handler, start_key);
		}

		if (ImGuiEx::KeyCodeInput(translation.get("InputStopKey").c_str(), stop_key, Language::English, current_hkl)) {
			keybind_handler.UpdateKey(stop_key_handler, stop_key);
		}

		if (ImGuiEx::KeyCodeInput(translation.get("InputResetKey").c_str(), reset_key, Language::English, current_hkl)) {
			keybind_handler.UpdateKey(reset_key_handler, reset_key);
		}

		if (ImGuiEx::KeyCodeInput(translation.get("InputPrepareKey").c_str(), prepare_key, Language::English, current_hkl)) {
			keybind_handler.UpdateKey(prepare_key_handler, prepare_key);
		}

		if (ImGuiEx::KeyCodeInput(translation.get("InputSegmentKey").c_str(), segment_key, Language::English, current_hkl)) {
			keybind_handler.UpdateKey(segment_key_handler, segment_key);
		}

		ImGui::PopStyleVar();
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem(translation.get("TabAppearence").c_str())) {
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0.f, 0.f });

		ImGui::Checkbox(translation.get("InputHideTimerButtons").c_str(), &hide_timer_buttons);
		ImGui::Checkbox(translation.get("InputHideSegmentButtons").c_str(), &hide_segment_buttons);
		ImGui::Checkbox(translation.get("InputUnifiedWindow").c_str(), &unified_window);

		if (!unified_window) {
			ImGui::Checkbox(translation.get("InputSegmentWindowBorder").c_str(), &segment_window_border);
		}

		ImGui::Separator();

		if (!hide_timer_buttons) {
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
		}

		if (!hide_segment_buttons) {
			if (ImGui::ColorButton(translation.get("InputSegmentButtonColor").c_str(), segment_button_color)) {
				ImGui::OpenPopup("##popupsegmentcolour");
			}
			ImGui::SameLine();
			ImGui::LabelText("##segmentbuttoncolor", translation.get("InputSegmentButtonColor").c_str());

			if (ImGui::ColorButton(translation.get("InputClearButtonColor").c_str(), clear_button_color)) {
				ImGui::OpenPopup("##popupclearcolour");
			}
			ImGui::SameLine();
			ImGui::LabelText("##clearbuttoncolor", translation.get("InputClearButtonColor").c_str());
		}

		ImGui::PopStyleVar();
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem(translation.get("TabFractalsDungeons").c_str())) {
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0.f, 0.f });

		ImGui::Checkbox(translation.get("InputSetEnabled").c_str(), &dungeon_fractal_settings.is_enabled);
		ImGui::Checkbox(translation.get("InputSetAutoPrepare").c_str(), &dungeon_fractal_settings.auto_prepare);
		ImGui::Checkbox(translation.get("InputSetAutoStop").c_str(), &dungeon_fractal_settings.auto_stop);

		ImGui::InputInt(translation.get("InputEarlyGG").c_str(), &dungeon_fractal_settings.early_gg_threshold);
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(translation.get("TooltipEarlyGG").c_str());
		}

		ImGui::PopStyleVar();
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem(translation.get("TabRaids").c_str())) {
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0.f, 0.f });

		ImGui::Checkbox(translation.get("InputSetEnabled").c_str(), &raid_settings.is_enabled);
		ImGui::Checkbox(translation.get("InputSetAutoPrepare").c_str(), &raid_settings.auto_prepare);
		ImGui::Checkbox(translation.get("InputSetAutoStop").c_str(), &raid_settings.auto_stop);

		ImGui::InputInt(translation.get("InputEarlyGG").c_str(), &raid_settings.early_gg_threshold);
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(translation.get("TooltipEarlyGG").c_str());
		}

		ImGui::PopStyleVar();
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem(translation.get("TabStrikes").c_str())) {
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0.f, 0.f });

		ImGui::Checkbox(translation.get("InputSetEnabled").c_str(), &strike_settings.is_enabled);
		ImGui::Checkbox(translation.get("InputSetAutoPrepare").c_str(), &strike_settings.auto_prepare);
		ImGui::Checkbox(translation.get("InputSetAutoStop").c_str(), &strike_settings.auto_stop);

		ImGui::InputInt(translation.get("InputEarlyGG").c_str(), &strike_settings.early_gg_threshold);
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip(translation.get("TooltipEarlyGG").c_str());
		}

		ImGui::PopStyleVar();
		ImGui::EndTabItem();
	}

	if (ImGui::BeginTabItem(translation.get("TabAbout").c_str())) {
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { 0.f, 0.f });

		ImGui::Text(translation.get("TextAbout1").c_str());
		ImGui::Dummy(ImVec2(0, 5));
		
		ImGui::Text(translation.get("TextAbout2").c_str());
		ImGui::SameLine();
		if (ImGui::SmallButton(translation.get("ButtonSource").c_str())) {
			ShellExecute(NULL, L"open", L"https://github.com/cordbleibaum/ArcDPS-Timer", NULL, NULL, SW_SHOWDEFAULT);
		}

		ImGui::Text(translation.get("TextAbout3").c_str());

		ImGui::PopStyleVar();
		ImGui::EndTabItem();
	}

	ImGui::EndTabBar();

	color_picker_popup("InputStartButtonColor", start_button_color);
	color_picker_popup("InputStopButtonColor", stop_button_color);
	color_picker_popup("InputResetButtonColor", reset_button_color);
	color_picker_popup("InputPrepareButtonColor", prepare_button_color);
	color_picker_popup("InputSegmentButtonColor", segment_button_color);
	color_picker_popup("InputClearButtonColor", clear_button_color);
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

bool Settings::is_enabled() const {
	if (disable_outside_instances && mumble_link->getMumbleContext()->mapType != MapType::MAPTYPE_INSTANCE) {
		return false;
	}

	if (disable_outside_instances && disable_in_fractal_lobby && mumble_link->getMumbleContext()->mapId == 872) {
		return false;
	}

	std::optional<SettingsSet> current_set = get_current_set();
	if (current_set.has_value()) {
		return current_set.value().is_enabled;
	}

	return true;
}

bool Settings::should_autoprepare() const {
	if (mumble_link->getMumbleContext()->mapType != MapType::MAPTYPE_INSTANCE) {
		return false;
	}

	if (disable_outside_instances && disable_in_fractal_lobby && mumble_link->getMumbleContext()->mapId == 872) {
		return false;
	}

	bool result = auto_prepare;
	std::optional<SettingsSet> current_set = get_current_set();
	if (current_set.has_value()) {
		result &= current_set.value().auto_prepare;
	}

	return result;
}

bool Settings::should_autostop() const {
	bool result = auto_stop;

	std::optional<SettingsSet> current_set = get_current_set();
	if (current_set.has_value()) {
		result &= current_set.value().auto_stop;
	}

	return result;
}

int Settings::get_early_gg_threshold() const {
	std::optional<SettingsSet> current_set = get_current_set();
	if (current_set.has_value()) {
		return current_set.value().early_gg_threshold;
	}

	return 0;
}

void Settings::color_picker_popup(std::string text_key, ImVec4& color) {
	if (ImGui::BeginPopup(std::string("##"+text_key).c_str())) {
		float color_array[4] = { start_button_color.x, start_button_color.y, start_button_color.z, start_button_color.w };
		ImGui::ColorPicker4(translation.get(text_key).c_str(), color_array);
		color = ImVec4(color_array[0], color_array[1], color_array[2], color_array[3]);
		ImGui::EndPopup();
	}
}

std::optional<SettingsSet> Settings::get_current_set() const {
	switch (map_tracker.get_instance_type()) {
	case InstanceType::Dungeon:
	case InstanceType::Fractal:
		return dungeon_fractal_settings;
	case InstanceType::Raid:
		return raid_settings;
	case InstanceType::Strike:
		return strike_settings;
	default:
		return std::nullopt;
	}
}
