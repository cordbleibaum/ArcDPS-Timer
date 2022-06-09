#pragma once

#include "lang.h"
#include "imgui/imgui.h"

#include <Windows.h>
#include <string>
#include <nlohmann/json.hpp>
#include <set>

#include "unofficial_extras/KeyBindStructs.h"
#include "arcdps-extension/KeyBindHandler.h"

namespace nlohmann {
	template <>
	struct adl_serializer<ImVec4> {
		static void to_json(json& j, const ImVec4& vec) {
			j["x"] = vec.x;
			j["y"] = vec.y;
			j["z"] = vec.z;
			j["w"] = vec.w;
		}

		static void from_json(const json& j, ImVec4& vec) {
			if (j.is_null()) {
				vec = ImVec4(0, 0, 0, 0);
			}
			else {
				j.at("x").get_to(vec.x);
				j.at("y").get_to(vec.y);
				j.at("z").get_to(vec.z);
				j.at("w").get_to(vec.w);
			}
		}
	};

	template <>
	struct adl_serializer<KeyBinds::Key> {
		static void to_json(json& j, const KeyBinds::Key& key) {
			j["code"] = key.Code;
			j["device_type"] = key.DeviceType;
			j["modifier"] = key.Modifier;
		}

		static void from_json(const json& j, KeyBinds::Key& key) {
			if (j.is_null()) {
				key = KeyBinds::Key();
			}
			else {
				j.at("code").get_to(key.Code);
				j.at("device_type").get_to(key.DeviceType);
				j.at("modifier").get_to(key.Modifier);
			}
		}
	};
}

class Settings {
public:
	Settings(std::string file, Translation& translation, KeyBindHandler& keybind_handler);
	void save();
	void mod_options();
	void mod_windows();
	bool mod_wnd(HWND pWindowHandle, UINT pMessage, WPARAM pAdditionalW, LPARAM pAdditionalL);

	bool show_timer;
	bool show_segments;
	bool auto_prepare;
	bool use_custom_id;
	bool unified_window;
	std::string custom_id;
	bool disable_outside_instances;
	std::string time_formatter;
	std::string segment_time_formatter;
	bool hide_timer_buttons;
	bool hide_segment_buttons;
	KeyBinds::Key start_key;
	KeyBinds::Key stop_key;
	KeyBinds::Key reset_key;
	KeyBinds::Key prepare_key;
	bool auto_stop;
	int early_gg_threshold;
	KeyBinds::Key segment_key;
	bool save_logs;
	std::set<int> additional_boss_ids;
	int boss_id_selected = -1;
	int boss_id_input = 0;

	ImVec4 start_button_color;
	ImVec4 stop_button_color;
	ImVec4 reset_button_color;
	ImVec4 prepare_button_color;

	uint64_t start_key_handler;
	uint64_t stop_key_handler;
	uint64_t reset_key_handler;
	uint64_t prepare_key_handler;
	uint64_t segment_key_handler;

	HKL current_hkl;
private:
	Translation& translation;
	KeyBindHandler& keybind_handler;

	int settings_version;
	std::string config_file;
};
