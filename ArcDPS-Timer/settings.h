#pragma once

#include "lang.h"
#include "imgui/imgui.h"

#include <Windows.h>
#include <string>
#include <nlohmann/json.hpp>


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
}

class Settings {
public:
	Settings(std::string file, Translation& translation);
	void save();
	void show_options();
	void show_windows();

	bool show_timer;
	bool show_segments;
	bool auto_prepare;
	bool use_custom_id;
	bool unified_window;
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
	bool save_logs;

	ImVec4 start_button_color;
	ImVec4 stop_button_color;
	ImVec4 reset_button_color;
	ImVec4 prepare_button_color;
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
