#pragma once

#include <nlohmann/json.hpp>
#include "imgui/imgui.h"

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
};