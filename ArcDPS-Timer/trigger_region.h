#pragma once

#include <Eigen/Dense>
#include <array>
#include <string>
#include <memory>
#include <nlohmann/json.hpp>
#include <boost/signals2.hpp>
#include <typeinfo>
#include <map>

#include "lang.h"
#include "mumble_link.h"

class TriggerRegion {
public:
	bool trigger(Eigen::Vector3f player_position);
	virtual bool check(Eigen::Vector3f player_position) = 0;
	virtual std::string get_typename_id() const = 0;
	virtual Eigen::Vector3f get_middle() const = 0;
	void reset();
	bool get_is_triggered() const;

	std::string name = "";
protected:
	bool is_triggered = false;
};

class SphereTrigger : public TriggerRegion {
public:
	SphereTrigger();
	SphereTrigger(Eigen::Vector3f position, float radius);
	virtual bool check(Eigen::Vector3f player_position) override;
	virtual std::string get_typename_id() const override;
	virtual Eigen::Vector3f get_middle() const override;

	Eigen::Vector3f position = Eigen::Vector3f::Zero();
	float radius = 0.0f;
};

class PlaneTrigger : public TriggerRegion {
public:
	PlaneTrigger();
	PlaneTrigger(Eigen::Vector2f side1, Eigen::Vector2f side2, float height, float z, float thickness);
	virtual bool check(Eigen::Vector3f player_position) override;
	virtual std::string get_typename_id() const override;
	virtual Eigen::Vector3f get_middle() const override;
	float height = 0.0f;
	float z = 0.0f;
	float thickness = 0.0f;
	Eigen::Vector2f side1 = Eigen::Vector2f::Zero();
	Eigen::Vector2f side2 = Eigen::Vector2f::Zero();
};

void region_to_json(nlohmann::json& j, const TriggerRegion* region);
void region_from_json(const nlohmann::json& j, TriggerRegion*& region);

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
			TriggerRegion* region;
			region_from_json(j, region);
			ptr = std::shared_ptr<TriggerRegion>(region);
		}
	};
}

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SphereTrigger, position, radius, name)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PlaneTrigger, side1, side2, height, z, thickness, name)

class TriggerWatcher {
public:
	TriggerWatcher(GW2MumbleLink& mumble_link);
	bool watch();
	void reset();
	void map_change(uint32_t map_id);
	~TriggerWatcher();

	boost::signals2::signal<void(std::string)> trigger_signal;

	std::vector<std::shared_ptr<TriggerRegion>> regions;
private:
	GW2MumbleLink& mumble_link;
	uint32_t last_map_id = 0;
	std::string trigger_directory = "addons/arcdps/arcdps-timer-triggers/";
};

class TriggerEditor {
public:
	TriggerEditor(Translation& translation, GW2MumbleLink& mumble_link, std::vector<std::shared_ptr<TriggerRegion>>& regions);
	void mod_imgui();
	void mod_windows();
	void map_change(uint32_t map_id);
private:
	GW2MumbleLink& mumble_link;
	Translation& translation;

	bool is_open = false;
	bool is_help_open = false;
	int selected_line = -1;

	float sphere_radius = 2;

	float plane_thickness = 1;
	float plane_z = 0;
	float plane_height = 2;
	std::array<float, 2> plane_position1 = {0};
	std::array<float, 2> plane_position2 = {0};

	std::vector<std::shared_ptr<TriggerRegion>>& regions;
	std::string region_name;

	std::map<std::string, std::vector<std::shared_ptr<TriggerRegion>>> trigger_sets;
	std::string trigger_set_directory = "addons/arcdps/arcdps-timer-triggers-sets/";
	std::string set_name;

	void save_sets();
};
