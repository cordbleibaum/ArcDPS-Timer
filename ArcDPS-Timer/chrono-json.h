#pragma once

#include <chrono>
#include <nlohmann/json.hpp>

namespace nlohmann {
	template<typename Clock, typename Duration>
	struct adl_serializer<std::chrono::time_point<Clock, Duration>> {
		static void to_json(json& j, const std::chrono::time_point<Clock, Duration>& tp) {
			j = std::format("{:%FT%T}", std::chrono::floor<std::chrono::milliseconds>(tp));
		}
	};
};
