#pragma once

#include <nlohmann/json.hpp>
#include <Eigen/Dense>

namespace nlohmann {
	template <>
	struct adl_serializer<Eigen::Vector3f> {
		static void to_json(json& j, const Eigen::Vector3f& vec) {
			j["x"] = vec.x();
			j["y"] = vec.y();
			j["z"] = vec.z();
		}

		static void from_json(const json& j, Eigen::Vector3f& vec) {
			if (j.is_null()) {
				vec = Eigen::Vector3f::Zero();
			}
			else {
				j.at("x").get_to(vec.x());
				j.at("y").get_to(vec.y());
				j.at("z").get_to(vec.z());
			}
		}
	};

	template <>
	struct adl_serializer<Eigen::Vector2f> {
		static void to_json(json& j, const Eigen::Vector2f& vec) {
			j["x"] = vec.x();
			j["y"] = vec.y();
		}

		static void from_json(const json& j, Eigen::Vector2f& vec) {
			if (j.is_null()) {
				vec = Eigen::Vector2f::Zero();
			}
			else {
				j.at("x").get_to(vec.x());
				j.at("y").get_to(vec.y());
			}
		}
	};
};