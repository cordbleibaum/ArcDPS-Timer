#pragma once

#include "lang.h"
#include "maptracker.h"
#include "util.h"

#include <Windows.h>
#include <string>
#include <nlohmann/json.hpp>
#include <set>

#include "keybind-json.h"

#include "imgui-json.h"

struct SettingsSet {
	bool is_enabled = true;
	bool auto_prepare = false;
	bool auto_stop = true;
	int early_gg_threshold = 10;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SettingsSet, is_enabled, auto_prepare, auto_stop, early_gg_threshold)

class Settings {
public:
	Settings(std::string file, const Translation& translation, KeyBindHandler& keybind_handler, const MapTracker& map_tracker, GW2MumbleLink& mumble_link);
	void mod_release();
	void mod_options();
	void mod_windows();
	bool mod_wnd(HWND pWindowHandle, UINT pMessage, WPARAM pAdditionalW, LPARAM pAdditionalL);

	bool is_enabled() const;
	bool should_autoprepare() const;
	bool should_autostop() const;
	int get_early_gg_threshold() const;

	bool show_timer;
	bool show_segments;
	bool show_history;
	bool use_custom_id;
	bool unified_window;
	std::string custom_id;
	std::string time_formatter;
	std::string segment_time_formatter;
	bool hide_timer_buttons;
	bool hide_segment_buttons;
	bool segment_window_border;
	KeyBinds::Key start_key;
	KeyBinds::Key stop_key;
	KeyBinds::Key reset_key;
	KeyBinds::Key prepare_key;
	KeyBinds::Key segment_key;
	bool save_logs;
	std::set<int> additional_boss_ids;
	int boss_id_selected = -1;
	int boss_id_input = 0;

	ImVec4 start_button_color;
	ImVec4 stop_button_color;
	ImVec4 reset_button_color;
	ImVec4 prepare_button_color;
	ImVec4 segment_button_color;
	ImVec4 clear_button_color;

	uint64_t start_key_handler;
	uint64_t stop_key_handler;
	uint64_t reset_key_handler;
	uint64_t prepare_key_handler;
	uint64_t segment_key_handler;

	HKL current_hkl;
private:
	const Translation& translation;
	KeyBindHandler& keybind_handler;
	const MapTracker& map_tracker;
	GW2MumbleLink& mumble_link;

	bool disable_outside_instances;
	bool disable_in_fractal_lobby;
	bool auto_prepare;
	bool auto_stop;

	int settings_version;
	std::string config_file;

	SettingsSet dungeon_fractal_settings;
	SettingsSet raid_settings;
	SettingsSet strike_settings;

	void color_picker_popup(std::string text_key, std::string popup_key, ImVec4& color);
	std::optional<SettingsSet> get_current_set() const;
};
