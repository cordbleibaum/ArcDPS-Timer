#pragma once

#include <Eigen/Dense>
#include <array>
#include <string>
#include <memory>
#include <nlohmann/json.hpp>
#include <typeinfo>

#include "lang.h"
#include "mumble_link.h"

class PlaneTrigger;
class SphereTrigger; 
class TriggerRegion;

class TriggerRegion {
public:
	bool trigger(Eigen::Vector3f player_position);
	virtual bool check(Eigen::Vector3f player_position) = 0;
	virtual std::string get_typename_id() const = 0;
	virtual Eigen::Vector3f get_middle() = 0;
	void reset();
protected:
	bool is_triggered;
};

class SphereTrigger : public TriggerRegion {
public:
	SphereTrigger(Eigen::Vector3f position, float radius);
	virtual bool check(Eigen::Vector3f player_position) override;
	virtual std::string get_typename_id() const override;
	virtual Eigen::Vector3f get_middle() override;

	Eigen::Vector3f position;
	float radius;
};

class PlaneTrigger : public TriggerRegion {
public:
	PlaneTrigger(Eigen::Vector2f side1, Eigen::Vector2f side2, float height, float z, float thickness);
	virtual bool check(Eigen::Vector3f player_position) override;
	virtual std::string get_typename_id() const override;
	virtual Eigen::Vector3f get_middle() override;
	float height, z, thickness;
	Eigen::Vector2f side1;
	Eigen::Vector2f side2;
};

void region_to_json(nlohmann::json& j, const TriggerRegion* region);

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

	template <>
	struct adl_serializer<std::shared_ptr<TriggerRegion>> {
		static void to_json(json& j, const std::shared_ptr<TriggerRegion>& ptr) {
			region_to_json(j, (TriggerRegion*) ptr.get());
		}

		static void from_json(const json& j, std::shared_ptr<TriggerRegion>& ptr) {
			//region_from_json(j, (TriggerRegion*)ptr.get());
		}
	};
}

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SphereTrigger, position, radius)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PlaneTrigger, side1, side2, height, z, thickness)

class TriggerWatcher {
public:
	TriggerWatcher(GW2MumbleLink& mumble_link);
	void watch();
	int get_last_triggered();

	std::vector<std::shared_ptr<TriggerRegion>> regions;
private:
	GW2MumbleLink& mumble_link;
	int last_triggered = -1;
};

class TriggerEditor {
public:
	TriggerEditor(Translation& translation, GW2MumbleLink& mumble_link);
	void mod_options();
	void mod_imgui();
private:
	GW2MumbleLink& mumble_link;
	Translation& translation;

	bool is_open = false;
	int selected_line = -1;

	float sphere_radius = 1;
	std::array<float, 3> sphere_position;

	float plane_thickness = 1;
	float plane_z = 0;
	float plane_height = 2;
	std::array<float, 2> plane_position1;
	std::array<float, 2> plane_position2;

	std::vector<std::shared_ptr<TriggerRegion>> regions;
};
